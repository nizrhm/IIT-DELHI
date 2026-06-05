#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    PUSH  = 0x01, 
    POP   = 0x02,
    DUP   = 0x03, 
    ADD   = 0x10, 
    SUB   = 0x11, 
    MUL   = 0x12, 
    DIV   = 0x13, 
    CMP   = 0x14, 
    JMP   = 0x20, 
    JZ    = 0x21, 
    JNZ   = 0x22, 
    STORE = 0x30, 
    LOAD  = 0x31, 
    CALL  = 0x40, 
    RET   = 0x41, 
    HALT  = 0xFF  
} OpCode;

#define STACK_SIZE 256
#define MEMORY_SIZE 1024
#define RETURN_STACK_SIZE 64

typedef struct {
    int stack[STACK_SIZE];
    int memory[MEMORY_SIZE];
    int return_stack[RETURN_STACK_SIZE];
    int pc;   
    int sp;   
    int rsp;  
    bool running;
} VM;

void vm_push(VM *vm, int val) {
    if (vm->sp >= STACK_SIZE - 1) {
        fprintf(stderr, "Error: Stack Overflow\n");
        vm->running = false;
        return;
    }
    vm->stack[++vm->sp] = val;
}

int vm_pop(VM *vm) {
    if (vm->sp < 0) {
        fprintf(stderr, "Error: Stack Underflow\n");
        vm->running = false;
        return 0;
    }
    return vm->stack[vm->sp--];
}

void vm_legacy_execute(VM *vm, int *program) {
    while (vm->running) {
        int opcode = program[vm->pc++];

        switch (opcode) {
            case PUSH:
                vm_push(vm, program[vm->pc++]);
                break;
            case POP:
                vm_pop(vm);
                break;
            case DUP: {
                int val = vm_pop(vm);
                vm_push(vm, val);
                vm_push(vm, val);
                break;
            }
            case ADD: {
                int b = vm_pop(vm);
                int a = vm_pop(vm);
                vm_push(vm, a + b);
                break;
            }
            case SUB: {
                int b = vm_pop(vm);
                int a = vm_pop(vm);
                vm_push(vm, a - b);
                break;
            }
            case MUL: {
                int b = vm_pop(vm);
                int a = vm_pop(vm);
                vm_push(vm, a * b);
                break;
            }
            case DIV: {
                int b = vm_pop(vm);
                int a = vm_pop(vm);
                if (b == 0) { printf("Error: Div by 0\n"); vm->running = false; break; }
                vm_push(vm, a / b);
                break;
            }
            case CMP: {
                int b = vm_pop(vm);
                int a = vm_pop(vm);
                vm_push(vm, (a < b) ? 1 : 0);
                break;
            }
            case JMP: {
                int addr = program[vm->pc++]; 
                vm->pc = addr;
                break;
            }
            case JZ: {
                int addr = program[vm->pc++];
                if (vm_pop(vm) == 0) vm->pc = addr;
                break;
            }
            case JNZ: {
                int addr = program[vm->pc++];
                if (vm_pop(vm) != 0) vm->pc = addr;
                break;
            }
            case STORE: {
                int idx = program[vm->pc++];
                vm->memory[idx] = vm_pop(vm);
                break;
            }
            case LOAD: {
                int idx = program[vm->pc++];
                vm_push(vm, vm->memory[idx]);
                break;
            }
            case CALL: {
                if (vm->rsp >= RETURN_STACK_SIZE - 1) {
                    fprintf(stderr, "Error: Return Stack Overflow\n");
                    vm->running = false;
                    break;
                }
                int addr = program[vm->pc++];
                vm->return_stack[++vm->rsp] = vm->pc; 
                vm->pc = addr;
                break;
            }
            case RET:
                if (vm->rsp < 0) {
                    fprintf(stderr, "Error: Return Stack Underflow\n");
                    vm->running = false;
                    break;
                }
                vm->pc = vm->return_stack[vm->rsp--];
                break;
            case HALT:
                vm->running = false;
                break;
            default:
                printf("Unknown Opcode: %02x at PC %d\n", opcode, vm->pc - 1);
                vm->running = false;
                break;
        }
    }
}

int* load_bytecode(const char* filename, int* program_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening bytecode file");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    int* program = malloc(fsize);
    if (!program) {
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(program, sizeof(int), fsize / sizeof(int), file);
    *program_size = (int)read_count;
    fclose(file);
    return program;
}

void vm_run(VM* vm, int* bytecode) {
     while (vm->running) {
         vm_legacy_execute(vm, bytecode); 
     }
}