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

typedef struct {
    uint64_t address;          
    uint8_t original_data;     
    int enabled;               
} breakpoint_t;

#define MAX_BREAKPOINTS 10
breakpoint_t active_breakpoints[MAX_BREAKPOINTS];
int bp_count = 0;

void run_debuggee(const char* program_path);
void run_debugger(pid_t child_pid);
void enable_breakpoint(pid_t pid, uint64_t addr);
void disable_breakpoint(pid_t pid, uint64_t addr, uint8_t original_data);
void display_registers(pid_t pid);
void display_status(pid_t pid, int status);
uint64_t get_rip(pid_t pid);
void set_rip(pid_t pid, uint64_t new_rip);

breakpoint_t* find_breakpoint(uint64_t addr);
void handle_breakpoint_hit(pid_t pid);
int step_over_breakpoint(pid_t pid, breakpoint_t *bp); 

void read_memory(pid_t pid, uint64_t addr, size_t count);


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

    printf("Breakpoint stepped over. Now stopped after original instruction.\n");
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
        if (i % 32 == 0) { 
            printf("\n0x%016lx: ", addr + i);
        }
        
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
    printf("RIP: 0x%016llx (Instruction Pointer)\t\t", regs.rip);
    printf("RSP: 0x%016llx (Stack Pointer)\n", regs.rsp);
    printf("RBP: 0x%016llx (Base Pointer)\t\t", regs.rbp);
    printf("RAX: 0x%016llx\n", regs.rax);
    printf("RBX: 0x%016llx\t\t\tRCX: 0x%016llx\n", regs.rbx, regs.rcx);
    printf("RDX: 0x%016llx\t\t\tRDI: 0x%016llx\n", regs.rdx, regs.rdi);
    printf("RSI: 0x%016llx\n", regs.rsi);
    printf("R8 : 0x%016llx\t\tR12: 0x%016llx\n", regs.r8, regs.r12);
    printf("R9 : 0x%016llx\t\tR13: 0x%016llx\n", regs.r9, regs.r13);
    printf("R10: 0x%016llx\t\tR14: 0x%016llx\n", regs.r10, regs.r14);
    printf("R11: 0x%016llx\t\tR15: 0x%016llx\n", regs.r11, regs.r15);
    printf("---------------------\n");
}


void display_status(pid_t pid, int status) {
    printf("--- Process Status ---\n");
    if (WIFEXITED(status)) {
        printf("Status: Exited normally.\n");
        printf("Exit Code: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Status: Terminated by signal.\n");
        printf("Signal: %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        printf("Status: Stopped.\n");
        printf("Signal that caused stop: %d (0x%x)\n", WSTOPSIG(status), WSTOPSIG(status));
        printf("Current RIP: 0x%llx\n", get_rip(pid));
        
        if (WSTOPSIG(status) == SIGTRAP) {
             printf("Reason: Breakpoint/Trace event.\n");
        } else {
             printf("Reason: External Signal or other PTRACE event.\n");
        }

    } else {
        printf("Status: Unknown/Running (Last waitpid status: %d)\n", status);
    }
    printf("----------------------\n");
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
    
    printf("Debuggee stopped immediately after loading.\n");

    while (WIFSTOPPED(status)) {
        char command[256];
        printf("\n(minigdb) ");
        if (fgets(command, sizeof(command), stdin) == NULL) break;
        
        command[strcspn(command, "\n")] = 0; 
        
        char *cmd_start = command;
        while (isspace(*cmd_start)) cmd_start++;

        if (strncmp(cmd_start, "cont", 4) == 0) {
            
            if (WSTOPSIG(status) == SIGTRAP) {
                uint64_t bp_addr = get_rip(child_pid) - 1;
                breakpoint_t *bp = find_breakpoint(bp_addr);
                if (bp) {
                    status = step_over_breakpoint(child_pid, bp);
                    if (status == -1) break;
                    continue; 
                }
            }

            if (ptrace(PTRACE_CONT, child_pid, NULL, NULL) < 0) {
                perror("ptrace(PTRACE_CONT) failed");
                break;
            }
            
            if (waitpid(child_pid, &status, 0) < 0) {
                perror("waitpid failed during continue");
                break;
            }
            
            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
                handle_breakpoint_hit(child_pid);
            } else if (WIFSTOPPED(status)) {
                 printf("Process stopped due to signal %d (0x%x).\n", WSTOPSIG(status), WSTOPSIG(status));
            }


        } else if (strncmp(cmd_start, "break 0x", 8) == 0) {
            uint64_t addr;
            if (sscanf(cmd_start + 6, "%lx", &addr) == 1) {
                enable_breakpoint(child_pid, addr);
            } else {
                printf("Invalid address format. Use 'break 0xADDRESS'\n");
            }
            
        } else if (strncmp(cmd_start, "info regs", 9) == 0) {
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
                }
            } else {
                if (ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL) < 0) {
                    perror("ptrace(PTRACE_SINGLESTEP) failed");
                    break;
                }
                if (waitpid(child_pid, &status, 0) < 0) {
                    perror("waitpid failed during simple single step");
                    break;
                }
            }
            
            if (WIFSTOPPED(status)) {
                 printf("Single instruction executed. Stopped at 0x%llx.\n", get_rip(child_pid));
                 display_registers(child_pid);
            }

        } else if (strncmp(cmd_start, "x ", 2) == 0) { 
            
            uint64_t addr = 0;
            size_t count = 64; 
            
            int parsed_items = 0;
            char *ptr = cmd_start + 1; 
            
            if (*ptr == '/') {
                parsed_items += sscanf(ptr, "/%zu", &count);
            }
            
            char *addr_str = strstr(cmd_start, "0x");
            
            if (addr_str) {
                if (sscanf(addr_str, "%lx", &addr) == 1) {
                    read_memory(child_pid, addr, count);
                } else {
                    printf("Invalid address format. Use 'x [/count] 0xADDRESS'\n");
                }
            } else {
                 printf("Invalid examine format. Use 'x [/count] 0xADDRESS'\n");
            }
            // --------------------------------------------------------------------

        } else if (strncmp(cmd_start, "quit", 4) == 0 || strncmp(cmd_start, "q", 1) == 0) {
            printf("Terminating debuggee.\n");
            kill(child_pid, SIGKILL);
            break;
        } else {
            printf("Unknown command. Supported: 'break 0x...', 'cont', 'info regs', 'step/s', 'status', 'x [/count] 0x...', 'quit/q'.\n");
        }
    }

    if (WIFEXITED(status)) {
        printf("Debuggee exited normally with status %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Debuggee terminated by signal %d\n", WTERMSIG(status));
    }
}

void debug_session_start(int target_pid) {
    pid_t child_pid = (pid_t)target_pid;

    if (child_pid <= 0) {
        fprintf(stderr, "Invalid PID for debugging.\n");
        return; 
    }

    printf("Debugging process %d\n", child_pid);

    run_debugger(child_pid);
}