#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../interface.h"

void start_debugger(int pid) {
    Process* p = get_process(pid);
    if (!p) return;

    // OUTPUT FIXED TO MATCH TEST:
    printf("Debugger started for PID %d.\n", pid);
    printf("Commands: step, continue, break <line>, run, regs, quit\n");
    
    char line[256];
    while (1) {
        printf("(debug) ");
        if (!fgets(line, 256, stdin)) break;
        
        if (strncmp(line, "step", 4) == 0) {
            vm_step(p->vm);
            printf("Executed 1 instr. PC: %d, Line: %d\n", p->vm->pc, p->vm->debug_meta.line_map[p->vm->pc]);
            
        } else if (strncmp(line, "run", 3) == 0) {
            vm_run(p->vm);
            
        } else if (strncmp(line, "regs", 4) == 0) {
            printf("PC: %d, SP: %d, Heap: %d\n", p->vm->pc, p->vm->sp, p->vm->numObjects);

        } else if (strncmp(line, "break", 5) == 0) {
            int line_num = atoi(line + 6);
            if (line_num > 0) vm_toggle_breakpoint(p->vm, line_num);
            else printf("Usage: break <line>\n");

        } else if (strncmp(line, "continue", 8) == 0) {
            printf("Continuing execution...\n");
            vm_continue(p->vm);

        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        }
    }
}