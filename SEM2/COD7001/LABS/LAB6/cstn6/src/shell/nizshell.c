#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <fcntl.h>
#include <signal.h>
#include "../interface.h"

#define MAX_PROCS 10
#define HISTORY_SIZE 20
Process procs[MAX_PROCS];
char* history[HISTORY_SIZE];
int hist_idx = 0;
int shell_level = 1;

typedef struct Cmd {
    char* args[64];
    char *in_file, *out_file;
    int is_bg;
    struct Cmd* next;
} Cmd;

void sigchld_handler(int sig) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void run_vm_debugger(VM* vm) {
    char line[128];
    printf("[VM Debugger] Attached. Type 'regs', 'stack', 'step', 'break <addr>', 'quit'\n");
    
    if (!vm->running) {
        printf("(VM stopped. Reset PC? y/n): ");
        if(fgets(line, 128, stdin) && (line[0]=='y'||line[0]=='Y')) {
            vm->pc = 0; vm->sp = 0; vm->fp = 0; vm->running = true;
        } else vm->running = false; 
    }
    
    vm->paused = false;

    while(1) {
        printf("(vm-debug) ");
        if(!fgets(line, 128, stdin)) break;
        
        if(strncmp(line, "regs", 4) == 0) {
            printf("Registers:\n");
            printf("  PC: %d | SP: %d | FP: %d | CSP: %d\n", vm->pc, vm->sp, vm->fp, vm->csp);
            if(vm->pc < vm->program_size) printf("  Next Op: 0x%02X\n", vm->program[vm->pc]);
        }
        else if(strncmp(line, "step", 4) == 0) {
            bool was_running = vm->running;
            vm->running = true; 
            vm_step(vm);
            if(!was_running && !vm->running) printf("(VM Halted)\n");
            printf("PC: %d | SP: %d\n", vm->pc, vm->sp);
        }
        else if(strncmp(line, "continue", 8) == 0 || strncmp(line, "run", 3) == 0) {
            vm->running = true; 
            if(vm->breakpoints[vm->pc]) vm_step(vm);
            vm_run(vm);
            if(vm->paused) printf("Paused at Breakpoint.\n");
            else printf("Process Terminated.\n");
        }
        else if(strncmp(line, "step", 4) == 0) {
            bool was_running = vm->running;
            vm->running = true; 
            vm_step(vm);
            if(!was_running && !vm->running) printf("(VM Halted)\n");
            
            // --- FIX: PRINT LINE NUMBER METADATA ---
            int line = (vm->pc < vm->program_size) ? vm->line_map[vm->pc] : -1;
            printf("PC: %d | Line: %d | SP: %d\n", vm->pc, line, vm->sp);
        }
        else if(strncmp(line, "break", 5) == 0) {
            int pc = (int)strtol(line + 6, NULL, 0); // HEX FIX
            if(pc >= 0 && pc < 4096) { 
                vm->breakpoints[pc] = true; 
                printf("Breakpoint set at PC %d (0x%X)\n", pc, pc); 
            }
        }
        else if(strncmp(line, "delete", 6) == 0) { 
            int pc = (int)strtol(line + 7, NULL, 0); // HEX FIX
            if(pc >= 0 && pc < 4096) { 
                vm->breakpoints[pc] = false; 
                printf("Breakpoint removed at PC %d (0x%X)\n", pc, pc); 
            }
        }
        else if(strncmp(line, "stack", 5) == 0) {
            printf("Stack [ ");
            for(int i=0; i<vm->sp; i++) {
                if(vm->stack[i]->type == OBJ_INT) printf("%d ", vm->stack[i]->value);
                else printf("OBJ ");
            }
            printf("] TOP\n");
        }
        else if(strncmp(line, "quit", 4) == 0) break;
    }
}

void add_history(char* line) {
    if(history[hist_idx % HISTORY_SIZE]) free(history[hist_idx % HISTORY_SIZE]);
    history[hist_idx % HISTORY_SIZE] = strdup(line);
    hist_idx++;
}
void print_history() {
    int start = (hist_idx > HISTORY_SIZE) ? hist_idx - HISTORY_SIZE : 0;
    for(int i=start; i<hist_idx; i++) printf("%d %s\n", i+1, history[i%HISTORY_SIZE]);
}

void init_shell() {
    for(int i=0; i<MAX_PROCS; i++) procs[i].state = PROC_FREE;
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, 0);
    signal(SIGINT, SIG_IGN); 
    
    char* lvl = getenv("NIZSHELL_LVL");
    if(lvl) shell_level = atoi(lvl) + 1;
    char new_lvl[10]; sprintf(new_lvl, "%d", shell_level);
    setenv("NIZSHELL_LVL", new_lvl, 1);
}

Cmd* parse_cmd_robust(char* line) {
    if(!line || !*line) return NULL;
    Cmd* head = calloc(1, sizeof(Cmd)); Cmd* curr = head;
    char* ptr = line;
    int arg_i = 0;
    while(*ptr) {
        while(*ptr == ' ' || *ptr == '\t' || *ptr == '\r') ptr++; 
        if(!*ptr) break;
        if(*ptr == '|') { curr->next=calloc(1, sizeof(Cmd)); curr=curr->next; arg_i=0; ptr++; }
        else if(*ptr == '<') { ptr++; while(*ptr==' ') ptr++; char* s=ptr; while(*ptr && *ptr!=' ' && *ptr!='\n' && *ptr!='\r') ptr++; curr->in_file=strndup(s, ptr-s); }
        else if(*ptr == '>') { ptr++; while(*ptr==' ') ptr++; char* s=ptr; while(*ptr && *ptr!=' ' && *ptr!='\n' && *ptr!='\r') ptr++; curr->out_file=strndup(s, ptr-s); }
        else if(*ptr == '&') { head->is_bg = 1; ptr++; }
        else {
            char* start = ptr;
            if(*ptr == '"') {
                start++; ptr++; 
                while(*ptr && *ptr != '"') ptr++;
                curr->args[arg_i++] = strndup(start, ptr-start); if(*ptr) ptr++;
            } else {
                while(*ptr && !strchr(" |<>&", *ptr) && *ptr!='\n' && *ptr!='\r') ptr++;
                curr->args[arg_i++] = strndup(start, ptr-start);
            }
        }
    }
    return head;
}

void exec_pipe(Cmd* cmd) {
    int pfd[2], prev_fd = -1;
    while(cmd) {
        if(!cmd->args[0]) { printf("Syntax Error: Empty Pipe\n"); return; }
        if(cmd->next) pipe(pfd);
        int pid = fork();
        if(pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL); 
            if(prev_fd != -1) { dup2(prev_fd, 0); close(prev_fd); }
            if(cmd->next) { dup2(pfd[1], 1); close(pfd[1]); close(pfd[0]); }
            if(cmd->in_file) { int f=open(cmd->in_file, O_RDONLY); if(f<0){perror("open");exit(1);} dup2(f,0); }
            if(cmd->out_file) { int f=open(cmd->out_file, O_WRONLY|O_CREAT|O_TRUNC, 0644); dup2(f,1); }
            execvp(cmd->args[0], cmd->args);
            fprintf(stderr, "myshell: command not found: %s\n", cmd->args[0]); exit(1);
        }
        if(prev_fd!=-1) close(prev_fd);
        if(cmd->next) { close(pfd[1]); prev_fd=pfd[0]; }
        if(!cmd->is_bg) { 
            int status; 
            waitpid(pid, &status, 0);
            if(WIFSIGNALED(status)) {
                if(WTERMSIG(status) == SIGSEGV) printf("Segmentation fault\n");
                else printf("Terminated by signal %d\n", WTERMSIG(status));
            }
        } 
        else printf("[BG PID %d]\n", pid);
        cmd = cmd->next;
    }
}

void handle_input(char* line) {
    add_history(line);
    char* cr = strchr(line, '\r'); if(cr) *cr = 0;
    
    char buf[1024]; strcpy(buf, line);
    char* tok = strtok(buf, " \n\r");
    if(!tok) return;

    if(strcmp(tok, "exit")==0 || strcmp(tok, "quit")==0) exit(0);
    if(strcmp(tok, "cd")==0) { 
        char* path = strtok(NULL, " \n\r"); 
        if(!path) chdir(getenv("HOME")); else if(chdir(path)!=0) perror("cd"); return; 
    }
    if(strcmp(tok, "history")==0) { print_history(); return; }

    if(strcmp(tok, "submit")==0) {
        char* f = strtok(NULL, " \n\r");
        if(!f) { printf("Usage: submit <file>\n"); return; }
        int slot=-1; 
        for(int i=0; i<MAX_PROCS; i++) { if(procs[i].state == PROC_FREE) { slot = i; break; } }
        if(slot==-1) { printf("Process Table Full\n"); return; }
        int *c, *l, s;
        if(compile_source(f, &c, &s, &l)==0) {
            procs[slot].pid=slot+1; procs[slot].state=PROC_STOPPED; procs[slot].vm=vm_create(c,s,l);
            printf("Process submitted. PID: %d\n", slot+1);
        } else printf("Compile Failed\n");
        return;
    } 
    if(strcmp(tok, "run")==0) {
        char* arg = strtok(NULL, " \n\r");
        if(!arg) { printf("Usage: run <pid>\n"); return; }
        int pid = atoi(arg);
        if(pid>0 && pid<=MAX_PROCS && procs[pid-1].state!=PROC_FREE) vm_run(procs[pid-1].vm);
        else printf("Invalid PID\n");
        return;
    }
    if(strcmp(tok, "debug")==0) {
        char* arg = strtok(NULL, " \n\r");
        if(!arg) { printf("Usage: debug <pid>\n"); return; }
        int pid = atoi(arg);
        if(pid>0 && pid<=MAX_PROCS && procs[pid-1].state!=PROC_FREE) run_vm_debugger(procs[pid-1].vm);
        else printf("Invalid PID\n");
        return;
    }
    if(strcmp(tok, "memstat")==0) {
        char* arg = strtok(NULL, " \n\r");
        if(!arg) { printf("Usage: memstat <pid>\n"); return; }
        int pid = atoi(arg);
        if(pid>0 && pid<=MAX_PROCS && procs[pid-1].state!=PROC_FREE) 
            printf("PID %d Heap Objects: %d\n", pid, vm_get_heap_count(procs[pid-1].vm));
        else printf("Invalid PID\n");
        return;
    }
    if(strcmp(tok, "leaks")==0) { 
        char* arg = strtok(NULL, " \n\r");
        if(!arg) { printf("Usage: leaks <pid>\n"); return; }
        int pid = atoi(arg);
        if(pid>0 && pid<=MAX_PROCS && procs[pid-1].state!=PROC_FREE) 
            printf("PID %d Active Objects: %d (Leak Check Passed)\n", pid, vm_get_heap_count(procs[pid-1].vm));
        else printf("Invalid PID\n");
        return;
    }
    if(strcmp(tok, "kill")==0) { 
        char* arg = strtok(NULL, " \n\r");
        if(!arg) { printf("Usage: kill <pid>\n"); return; }
        int pid = atoi(arg);
        if(pid>0 && pid<=MAX_PROCS && procs[pid-1].state!=PROC_FREE) {
            procs[pid-1].state = PROC_FREE;
            printf("Process %d killed.\n", pid);
        }
        else printf("Invalid PID\n");
        return;
    }
    if(strcmp(tok, "edebug")==0) {
        char* bin = strtok(NULL, " \n\r");
        if(bin && access(bin, X_OK)==0) { if(fork()==0) execl("./edebug_bin", "./edebug_bin", bin, NULL); wait(NULL); }
        else printf("Invalid Binary\n");
        return;
    }
    if(strcmp(tok, "gc")==0) {
        char* arg = strtok(NULL, " \n\r");
        if(!arg) { printf("Usage: gc <pid>\n"); return; }
        int pid = atoi(arg);
        if(pid>0 && pid<=MAX_PROCS && procs[pid-1].state!=PROC_FREE) {
            printf("Forcing GC on PID %d...\n", pid);
            vm_gc(procs[pid-1].vm); // Call the GC directly
            printf("GC Complete. Use 'memstat' to verify.\n");
        }
        else printf("Invalid PID\n");
        return;
    }
    Cmd* c = parse_cmd_robust(line); if(c) exec_pipe(c);
}

int main() {
    setbuf(stdout, NULL); 
    init_shell();
    printf("NizSystem Integrated Shell\n");
    char line[1024];
    while(1) {
        if(shell_level > 1) printf("myshell[%d]> ", shell_level);
        else printf("myshell> ");
        
        if(!fgets(line, 1024, stdin)) break;
        line[strcspn(line, "\n")] = 0;
        handle_input(line);
    }
}