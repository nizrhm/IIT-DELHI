#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../interface.h"

// --- OBJECT MANAGEMENT ---

Object* vm_allocate(VM* vm, ObjectType type) {
    // 1. Check Limits
    if (vm->numObjects >= MAX_OBJECTS) {
        // 2. Trigger GC if full
        vm_gc(vm);
        
        // 3. Check Limits AGAIN
        if (vm->numObjects >= MAX_OBJECTS) {
            printf("Error: Out of Memory (Heap Full)\n");
            vm->running = false; // Halt VM gracefully
            return NULL;
        }
    }

    // 4. Find free slot
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (vm->heap[i].type == OBJ_FREE) {
            vm->heap[i].type = type;
            vm->heap[i].marked = 0;
            vm->heap[i].next = vm->objects;
            vm->objects = &vm->heap[i];
            vm->numObjects++;
            return &vm->heap[i];
        }
    }
    return NULL; // Should not happen if count is correct
}

Object* vm_new_int(VM* vm, int val) {
    Object* obj = vm_allocate(vm, OBJ_INT);
    if (!obj) return NULL; // Safety check
    obj->value = val;
    return obj;
}

// --- GARBAGE COLLECTOR ---

void mark(Object* obj) {
    if (!obj || obj->marked) return;
    obj->marked = 1;
}

void mark_all(VM* vm) {
    // Mark Stack
    for (int i = 0; i < vm->sp; i++) mark(vm->stack[i]);
    // Mark Variables (Globals/Locals would be here if managed by VM objects)
}

void sweep(VM* vm) {
    Object** object = &vm->objects;
    while (*object) {
        if (!(*object)->marked) {
            Object* unreached = *object;
            *object = unreached->next;
            unreached->type = OBJ_FREE; // Return to pool
            vm->numObjects--;
        } else {
            (*object)->marked = 0; // Reset for next cycle
            object = &(*object)->next;
        }
    }
}

void vm_gc(VM* vm) {
    int start_count = vm->numObjects;
    mark_all(vm);
    sweep(vm);
    int reclaimed = start_count - vm->numObjects;
    if (reclaimed > 0) {
        // Only print if significant to avoid log spam
        // printf("[GC] Reclaimed %d objects.\n", reclaimed); 
    }
}

int vm_get_heap_count(VM* vm) {
    return vm->numObjects;
}

// --- VM EXECUTION ---

VM* vm_create(int* code, int code_size, int* lines) {
    VM* vm = calloc(1, sizeof(VM));
    vm->program = code;
    vm->program_size = code_size;
    vm->line_map = lines;
    vm->sp = 0;
    vm->fp = 0;
    vm->csp = 0;
    vm->running = false;
    vm->paused = false;
    
    // Initialize Heap Pool
    for(int i=0; i<MAX_OBJECTS; i++) vm->heap[i].type = OBJ_FREE;
    return vm;
}

void vm_step(VM* vm) {
    if (vm->pc >= vm->program_size) { vm->running = false; return; }
    if (vm->sp >= 256) { printf("Error: Stack Overflow\n"); vm->running = false; return; }

    int op = vm->program[vm->pc++];
    int arg = (vm->pc < vm->program_size) ? vm->program[vm->pc++] : 0;

    switch(op) {
        case OP_PUSH: 
            vm->stack[vm->sp++] = vm_new_int(vm, arg); 
            break;
        case OP_LOAD_L: // Load Local
            if (vm->fp + arg < vm->sp) vm->stack[vm->sp++] = vm->stack[vm->fp + arg];
            else { printf("Error: Stack Access Violation\n"); vm->running = false; }
            break;
        case OP_STORE_L: // Store Local
            if (vm->fp + arg < vm->sp) vm->stack[vm->fp + arg] = vm->stack[--vm->sp];
            else { printf("Error: Stack Access Violation\n"); vm->running = false; }
            break;
        case OP_LOAD_G: // Globals are just stored at bottom of stack for this VM model
            if (arg < 256) vm->stack[vm->sp++] = vm->stack[arg]; 
            break;
        case OP_STORE_G:
            if (arg < 256) vm->stack[arg] = vm->stack[--vm->sp];
            break;
            
        case OP_ADD: {
            if (vm->sp < 2) { printf("Error: Stack Underflow\n"); vm->running=false; return; }
            Object *b = vm->stack[--vm->sp], *a = vm->stack[--vm->sp];
            if(a && b) vm->stack[vm->sp++] = vm_new_int(vm, a->value + b->value);
            break; 
        }
        case OP_SUB: {
            if (vm->sp < 2) { printf("Error: Stack Underflow\n"); vm->running=false; return; }
            Object *b = vm->stack[--vm->sp], *a = vm->stack[--vm->sp];
            if(a && b) vm->stack[vm->sp++] = vm_new_int(vm, a->value - b->value);
            break; 
        }
        case OP_MUL: {
            if (vm->sp < 2) { printf("Error: Stack Underflow\n"); vm->running=false; return; }
            Object *b = vm->stack[--vm->sp], *a = vm->stack[--vm->sp];
            if(a && b) vm->stack[vm->sp++] = vm_new_int(vm, a->value * b->value);
            break; 
        }
        case OP_DIV: {
            if (vm->sp < 2) { printf("Error: Stack Underflow\n"); vm->running=false; return; }
            Object *b = vm->stack[--vm->sp], *a = vm->stack[--vm->sp];
            if (b && b->value == 0) { printf("Error: DivZero\n"); vm->running = false; return; }
            if(a && b) vm->stack[vm->sp++] = vm_new_int(vm, a->value / b->value);
            break; 
        }
        
        // Control Flow
        case OP_JMP: vm->pc = arg; break;
        case OP_JZ: {
            if (vm->sp < 1) { printf("Error: Stack Underflow\n"); vm->running=false; return; }
            Object* top = vm->stack[--vm->sp];
            if (top && top->value == 0) vm->pc = arg;
            break;
        }
        case OP_CMP: { // <
             Object *b = vm->stack[--vm->sp], *a = vm->stack[--vm->sp];
             vm->stack[vm->sp++] = vm_new_int(vm, (a->value < b->value) ? 1 : 0);
             break;
        }
        case OP_GT: { // >
             Object *b = vm->stack[--vm->sp], *a = vm->stack[--vm->sp];
             vm->stack[vm->sp++] = vm_new_int(vm, (a->value > b->value) ? 1 : 0);
             break;
        }
        case OP_EQ: { // ==
             Object *b = vm->stack[--vm->sp], *a = vm->stack[--vm->sp];
             vm->stack[vm->sp++] = vm_new_int(vm, (a->value == b->value) ? 1 : 0);
             break;
        }
        
        case OP_HALT: 
            vm->running = false; 
            printf("[VM] Halted.\n"); 
            break;
            
        default: printf("Unknown Opcode %d\n", op); vm->running = false;
    }
}

void vm_run(VM* vm) {
    if (!vm) return;
    vm->running = true;
    while(vm->running && !vm->paused) {
        if(vm->breakpoints[vm->pc]) {
            vm->paused = true;
            printf("[VM] Hit Breakpoint at PC %d\n", vm->pc);
            break;
        }
        vm_step(vm);
    }
}