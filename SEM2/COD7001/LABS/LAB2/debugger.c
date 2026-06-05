#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h> 
#include <errno.h>
#include <stdint.h>
#include <signal.h> 
#include <ctype.h>

// --- STRUCTURES ---
typedef struct {
    uint64_t address;          
    uint8_t original_data;     
    int enabled;               
} breakpoint_t;

#define MAX_BREAKPOINTS 10
breakpoint_t active_breakpoints[MAX_BREAKPOINTS];
int bp_count = 0;

// --- PROTOTYPES ---
void run_debuggee(const char* program_path);
void run_debugger(pid_t child_pid);
void enable_breakpoint(pid_t pid, uint64_t addr);
void disable_breakpoint(pid_t pid, uint64_t addr, uint8_t original_data);
void display_registers(pid_t pid);
void display_status(pid_t pid, int status);
void print_help();
uint64_t get_rip(pid_t pid);
void set_rip(pid_t pid, uint64_t new_rip);
breakpoint_t* find_breakpoint(uint64_t addr);
void handle_breakpoint_hit(pid_t pid);
int step_over_breakpoint(pid_t pid, breakpoint_t *bp); 
void read_memory(pid_t pid, uint64_t addr, size_t count);

// --- HELPERS ---

uint64_t get_rip(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) {
        if (errno != ESRCH) perror("PTRACE_GETREGS failed (get_rip)");
        return 0;
    }
    return regs.rip;
}

void set_rip(pid_t pid, uint64_t new_rip) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) {
        perror("PTRACE_GETREGS failed (set_rip)");
        return;
    }
    regs.rip = new_rip;
    if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) < 0) {
        perror("PTRACE_SETREGS failed (set_rip)");
    }
}

breakpoint_t* find_breakpoint(uint64_t addr) {
    for (int i = 0; i < bp_count; i++) {
        if (active_breakpoints[i].address == addr) {
            return &active_breakpoints[i];
        }
    }
    return NULL;
}

void enable_breakpoint(pid_t pid, uint64_t addr) {
    if (find_breakpoint(addr)) {
        printf("Breakpoint already exists at 0x%lx\n", addr);
        return;
    }
    if (bp_count >= MAX_BREAKPOINTS) {
        fprintf(stderr, "Maximum number of breakpoints reached.\n");
        return;
    }

    long data = ptrace(PTRACE_PEEKTEXT, pid, (void*)addr, NULL);
    if (data == -1 && errno != 0) {
        perror("PTRACE_PEEKTEXT failed");
        return;
    }

    uint8_t original_data = (uint8_t)(data & 0xFF);
    long trap = (data & ~0xFF) | 0xCC; 

    if (ptrace(PTRACE_POKETEXT, pid, (void*)addr, (void*)trap) < 0) {
        perror("PTRACE_POKETEXT failed to set trap");
        return;
    }

    active_breakpoints[bp_count].address = addr;
    active_breakpoints[bp_count].original_data = original_data;
    active_breakpoints[bp_count].enabled = 1;
    bp_count++;

    printf("Breakpoint set at 0x%lx (Original byte: 0x%x)\n", addr, original_data);
}

void disable_breakpoint(pid_t pid, uint64_t addr, uint8_t original_data) {
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void*)addr, NULL);
    if (data == -1 && errno != 0) {
        perror("PTRACE_PEEKTEXT failed to read for restore");
        return;
    }
    long restored = (data & ~0xFF) | original_data;
    if (ptrace(PTRACE_POKETEXT, pid, (void*)addr, (void*)restored) < 0) {
        perror("PTRACE_POKETEXT failed to restore instruction");
    }
}

int step_over_breakpoint(pid_t pid, breakpoint_t *bp) {
    int status;
    disable_breakpoint(pid, bp->address, bp->original_data);

    if (ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) < 0) {
        perror("PTRACE_SINGLESTEP failed during step over");
        return -1;
    }
    
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid failed after single step");
        return -1;
    }

    if (WIFSTOPPED(status)) {
        long data = ptrace(PTRACE_PEEKTEXT, pid, (void*)bp->address, NULL);
        long trap = (data & ~0xFF) | 0xCC;
        if (ptrace(PTRACE_POKETEXT, pid, (void*)bp->address, (void*)trap) < 0) {
            perror("PTRACE_POKETEXT failed to re-set trap");
        }
    }
    return status;
}

void handle_breakpoint_hit(pid_t pid) {
    uint64_t rip = get_rip(pid);
    uint64_t bp_addr = rip - 1; 

    breakpoint_t *bp = find_breakpoint(bp_addr);

    if (bp) {
        printf("\n*** Breakpoint hit at address 0x%lx ***\n", bp_addr);
        set_rip(pid, bp_addr);
    } else {
        printf("\nProcess stopped at 0x%lx due to SIGTRAP, but no matching breakpoint found.\n", rip);
    }
}

void read_memory(pid_t pid, uint64_t addr, size_t count) {
    size_t i;
    printf("--- Memory Dump: 0x%016lx to 0x%016lx ---\n", addr, addr + count - 1);
    for (i = 0; i < count; i += sizeof(long)) {
        if (i % 32 == 0) printf("\n0x%016lx: ", addr + i);
        long data = ptrace(PTRACE_PEEKDATA, pid, (void*)(addr + i), NULL);
        if (data == -1 && errno != 0) {
            perror("PTRACE_PEEKDATA failed");
            if (i % 32 != 0) printf("\n");
            return;
        }
        printf("%016lx ", data);
    }
    printf("\n-------------------------------------------------------------------\n");
}

void display_registers(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) {
        perror("PTRACE_GETREGS failed");
        return;
    }
    printf("\n--- CPU Registers ---\n");
    printf("RIP: 0x%016llx\n", regs.rip);
    printf("RSP: 0x%016llx\n", regs.rsp);
    printf("RBP: 0x%016llx\n", regs.rbp);
    printf("RAX: 0x%016llx\tRBX: 0x%016llx\n", regs.rax, regs.rbx);
    printf("RCX: 0x%016llx\tRDX: 0x%016llx\n", regs.rcx, regs.rdx);
    printf("RSI: 0x%016llx\tRDI: 0x%016llx\n", regs.rsi, regs.rdi);
    printf("---------------------\n");
}

void display_status(pid_t pid, int status) {
    if (WIFEXITED(status)) {
        printf("Status: Exited normally. Code: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Status: Terminated by signal: %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) != SIGTRAP) {
             printf("Status: Stopped by signal %d\n", WSTOPSIG(status));
        }
    }
}

void print_help() {
    printf("\n--- Available Commands ---\n");
    printf("  break 0xADDR    : Set breakpoint at hex address (e.g., break 0x401000)\n");
    printf("  cont            : Continue execution\n");
    printf("  step (or s)     : Single step instruction\n");
    printf("  regs            : Show CPU registers\n");
    printf("  status          : Show process status\n");
    printf("  x [/n] 0xADDR   : Examine n bytes of memory at address (e.g., x /16 0x402000)\n");
    printf("  quit (or q)     : Exit debugger\n");
    printf("  help            : Show this message\n");
    printf("--------------------------\n");
}

void run_debuggee(const char* program_path) {
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
        perror("ptrace(PTRACE_TRACEME) failed");
        exit(EXIT_FAILURE);
    }
    execl(program_path, program_path, NULL);
    perror("execl failed");
    exit(EXIT_FAILURE);
}

void run_debugger(pid_t child_pid) {
    int status;
    printf("Debugger started. Debuggee PID: %d\n", child_pid);

    if (waitpid(child_pid, &status, 0) < 0) {
        perror("waitpid failed during initial wait");
        return;
    }
    printf("Debuggee stopped at entry. Type 'help' for commands.\n");

    while (WIFSTOPPED(status)) {
        char command[256];
        printf("\n(edebug) ");
        if (fgets(command, sizeof(command), stdin) == NULL) break;
        
        command[strcspn(command, "\n")] = 0; 
        char *cmd_start = command;
        while (isspace(*cmd_start)) cmd_start++;
        
        if (*cmd_start == 0) continue; // Ignore empty lines

        if (strncmp(cmd_start, "cont", 4) == 0 || strncmp(cmd_start, "run", 3) == 0) {
            if (WSTOPSIG(status) == SIGTRAP) {
                uint64_t bp_addr = get_rip(child_pid) - 1;
                breakpoint_t *bp = find_breakpoint(bp_addr);
                if (bp) {
                    status = step_over_breakpoint(child_pid, bp);
                    if (status == -1) break;
                }
            }
            if (ptrace(PTRACE_CONT, child_pid, NULL, NULL) < 0) break;
            if (waitpid(child_pid, &status, 0) < 0) break;
            
            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) handle_breakpoint_hit(child_pid);
            else display_status(child_pid, status);

        } else if (strncmp(cmd_start, "break", 5) == 0) {
            uint64_t addr;
            char *arg = cmd_start + 5;
            while(isspace(*arg)) arg++;
            if (strncmp(arg, "0x", 2) == 0 && sscanf(arg, "%lx", &addr) == 1) enable_breakpoint(child_pid, addr);
            else printf("Error: Invalid syntax. Usage: break 0xADDRESS\n");
            
        } else if (strncmp(cmd_start, "info regs", 9) == 0 || strncmp(cmd_start, "regs", 4) == 0) {
            display_registers(child_pid);
            
        } else if (strncmp(cmd_start, "status", 6) == 0) {
            display_status(child_pid, status);

        } else if (strncmp(cmd_start, "step", 4) == 0 || strncmp(cmd_start, "s", 1) == 0) {
            if (WSTOPSIG(status) == SIGTRAP) {
                uint64_t bp_addr = get_rip(child_pid) - 1; 
                breakpoint_t *bp = find_breakpoint(bp_addr);
                if (bp) {
                    status = step_over_breakpoint(child_pid, bp);
                    if (status == -1) break;
                    continue; 
                }
            }
            if (ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL) < 0) break;
            if (waitpid(child_pid, &status, 0) < 0) break;
            if (WIFSTOPPED(status)) display_registers(child_pid);

        } else if (strncmp(cmd_start, "x", 1) == 0) { 
            uint64_t addr = 0;
            size_t count = 32; 
            char *ptr = cmd_start + 1; 
            while(isspace(*ptr)) ptr++;
            
            if (*ptr == '/') {
                ptr++;
                if (sscanf(ptr, "%zu", &count) == 1) {
                     while(isdigit(*ptr)) ptr++;
                     while(isspace(*ptr)) ptr++;
                }
            }
            
            if (strncmp(ptr, "0x", 2) == 0 && sscanf(ptr, "%lx", &addr) == 1) read_memory(child_pid, addr, count);
            else printf("Error: Invalid syntax. Usage: x [/count] 0xADDRESS\n");

        } else if (strncmp(cmd_start, "quit", 4) == 0 || strncmp(cmd_start, "q", 1) == 0) {
            printf("Terminating debuggee.\n");
            kill(child_pid, SIGKILL);
            break;
        } else if (strncmp(cmd_start, "help", 4) == 0) {
            print_help();
        } else {
            printf("Unknown command: '%s'\n", cmd_start);
            print_help(); // Automatically show correct usage
        }
    }

    if (WIFEXITED(status)) printf("Debuggee exited normally with status %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status)) printf("Debuggee terminated by signal %d\n", WTERMSIG(status));
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program_to_debug>\n", argv[0]);
        return EXIT_FAILURE;
    }
    pid_t pid = fork();
    if (pid == 0) run_debuggee(argv[1]);
    else if (pid > 0) run_debugger(pid);
    else return EXIT_FAILURE;
    return EXIT_SUCCESS;
}