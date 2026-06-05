#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>

#define MAX_BP 10
struct BP { long addr; long orig; int active; } bps[MAX_BP];

void set_bp(pid_t pid, long addr, int i) {
    long data = ptrace(PTRACE_PEEKTEXT, pid, addr, 0);
    bps[i].orig = data; bps[i].addr = addr; bps[i].active = 1;
    ptrace(PTRACE_POKETEXT, pid, addr, (data & ~0xFF) | 0xCC);
    printf("BP Set at %lx\n", addr);
}

void restore_bp(pid_t pid, int i) {
    long data = ptrace(PTRACE_PEEKTEXT, pid, bps[i].addr, 0);
    ptrace(PTRACE_POKETEXT, pid, bps[i].addr, (data & ~0xFF) | (bps[i].orig & 0xFF));
}

// --- NEW: Explicit Removal Command ---
void delete_bp(pid_t pid, long addr) {
    for(int i=0; i<MAX_BP; i++) {
        if(bps[i].active && bps[i].addr == addr) {
            restore_bp(pid, i);
            bps[i].active = 0;
            printf("BP Removed at %lx\n", addr);
            return;
        }
    }
    printf("No BP found at %lx\n", addr);
}

void run_debug(const char* prog) {
    pid_t pid = fork();
    if(pid == 0) { ptrace(PTRACE_TRACEME, 0,0,0); execl(prog, prog, NULL); exit(1); }
    
    int status; wait(&status);
    printf("Debugger attached to PID %d. Cmds: run, step, break <addr>, delete <addr>, regs, peek <addr>\n", pid);
    
    char line[128];
    while(printf("(edebug) "), fgets(line, 128, stdin)) {
        if(strncmp(line, "run", 3)==0 || strncmp(line, "cont", 4)==0) {
            ptrace(PTRACE_CONT, pid, 0, 0); wait(&status);
            if(WIFEXITED(status)) break;
            if(WIFSTOPPED(status)) {
                struct user_regs_struct r; ptrace(PTRACE_GETREGS, pid, 0, &r);
                long rip = r.rip - 1; // PC is 1 byte past 0xCC
                int hit = 0;
                for(int i=0; i<MAX_BP; i++) {
                    if(bps[i].active && bps[i].addr == rip) {
                        printf("Hit BP at %lx\n", rip);
                        restore_bp(pid, i);
                        r.rip = rip; ptrace(PTRACE_SETREGS, pid, 0, &r);
                        ptrace(PTRACE_SINGLESTEP, pid, 0, 0); wait(&status);
                        set_bp(pid, rip, i); // Re-enable for loop
                        hit = 1; break;
                    }
                }
                if(!hit) printf("Stopped (Signal %d)\n", WSTOPSIG(status));
            }
        }
        else if(strncmp(line, "step", 4)==0) {
            ptrace(PTRACE_SINGLESTEP, pid, 0, 0); wait(&status);
            if(WIFEXITED(status)) break;
            struct user_regs_struct r; ptrace(PTRACE_GETREGS, pid, 0, &r);
            printf("RIP: %llx\n", r.rip);
        }
        else if(strncmp(line, "break", 5)==0) {
            long addr = strtol(line+6, NULL, 16);
            int i=0; while(i<MAX_BP && bps[i].active) i++;
            if(i<MAX_BP && addr) set_bp(pid, addr, i); else printf("Max BPs reached or Invalid Addr\n");
        }
        else if(strncmp(line, "delete", 6)==0) { // <--- ADDED
            long addr = strtol(line+7, NULL, 16);
            delete_bp(pid, addr);
        }
        else if(strncmp(line, "regs", 4)==0) {
            struct user_regs_struct r; ptrace(PTRACE_GETREGS, pid, 0, &r);
            printf("RIP: %llx RSP: %llx RAX: %llx\n", r.rip, r.rsp, r.rax);
        }
        else if(strncmp(line, "peek", 4)==0) {
            long addr = strtol(line+5, NULL, 16);
            printf("Mem[%lx] = %lx\n", addr, ptrace(PTRACE_PEEKTEXT, pid, addr, 0));
        }
        else if(strncmp(line, "quit", 4)==0) { kill(pid, 9); break; }
    }
}
int main(int c, char** v) { if(c>1) run_debug(v[1]); else printf("Usage: edebug <bin>\n"); }