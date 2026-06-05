#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <signal.h> // For SIGINT
#include "../interface.h"

#define MAX_CMD_LEN 1024
#define MAX_PROCS 10

Process process_table[MAX_PROCS];

void init_shell() {
    for(int i=0; i<MAX_PROCS; i++) process_table[i].state = PROC_FREE;
    // LAB 1: Ignore Ctrl-C so shell stays alive
    signal(SIGINT, SIG_IGN); 
}

Process* get_process(int pid) {
    if (pid <= 0 || pid > MAX_PROCS) return NULL;
    Process* p = &process_table[pid-1];
    return (p->state == PROC_FREE) ? NULL : p;
}

void handle_cmd(char* input) {
    char* args[64];
    int arg_count = 0;
    char* token = strtok(input, " \n\t");
    while (token != NULL && arg_count < 63) {
        args[arg_count++] = token;
        token = strtok(NULL, " \n\t");
    }
    args[arg_count] = NULL;
    if (arg_count == 0) return;

    if (strcmp(args[0], "submit") == 0) {
        if (arg_count < 2) { printf("Usage: submit <file>\n"); return; }
        if (access(args[1], F_OK) != 0) { printf("Error: File '%s' not found.\n", args[1]); return; }
        int slot = -1;
        for(int i=0; i<MAX_PROCS; i++) if (process_table[i].state == PROC_FREE) { slot = i; break; }
        if (slot == -1) { printf("Error: Process table full.\n"); return; }

        int* code = NULL; int* lines = NULL; int size = 0;
        printf("[Shell] Invoking Parser/Compiler...\n");
        if (compile_source(args[1], &code, &size, &lines) == 0) {
            Process* p = &process_table[slot];
            p->pid = slot + 1;
            strncpy(p->filename, args[1], 63);
            p->state = PROC_STOPPED;
            p->vm = vm_create(code, size, lines);
            printf("Process submitted successfully. PID: %d\n", p->pid);
        } else { printf("Error: Compilation failed.\n"); }

    } else if (strcmp(args[0], "run") == 0) {
        int pid = atoi(args[1] ? : "0");
        Process* p = get_process(pid);
        if (!p) { printf("Error: Invalid PID.\n"); return; }
        if (p->state == PROC_TERMINATED) { printf("Error: PID %d already finished.\n", pid); return; }
        printf("Starting execution of PID %d...\n", pid);
        vm_run(p->vm);
        p->state = PROC_TERMINATED;

    } else if (strcmp(args[0], "debug") == 0) {
        int pid = atoi(args[1] ? : "0");
        if (get_process(pid)) start_debugger(pid);
        else printf("PID not found.\n");

    } else if (strcmp(args[0], "memstat") == 0) {
        int pid = atoi(args[1] ? : "0");
        Process* p = get_process(pid);
        if (p) printf("PID %d Heap: %d objects\n", pid, vm_get_heap_count(p->vm));
        else printf("PID not found.\n");

    } else if (strcmp(args[0], "exit") == 0) {
        exit(0);

    } else {
        // LAB 1: Standard Execution
        pid_t pid = fork();
        if (pid == 0) {
            // Child: Restore default SIGINT
            signal(SIGINT, SIG_DFL); 
            execvp(args[0], args);
            printf("myshell: command not found: %s\n", args[0]);
            exit(1);
        } else waitpid(pid, NULL, 0);
    }
}

int main() {
    init_shell();
    printf("MySystem Integrated Shell (Lab 1-6 Complete)\n");
    char line[MAX_CMD_LEN];
    while (1) {
        printf("myshell> ");
        if (!fgets(line, MAX_CMD_LEN, stdin)) break;
        handle_cmd(line);
    }
    return 0;
}