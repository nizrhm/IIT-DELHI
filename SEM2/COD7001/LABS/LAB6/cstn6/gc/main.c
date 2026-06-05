#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    OBJ_INT,
    OBJ_PAIR
} ObjType;

typedef struct Obj {
    ObjType type;
    bool marked;            
    struct Obj* next;       
    union {
        int value;          
        struct {
            struct Obj* left;
            struct Obj* right;
        } pair;             
    };
} Obj;

#define STACK_SIZE 1048576        
#define GC_WORKLIST_SIZE 1048576  
#define GLOBAL_SIZE 1024          

typedef struct {
    Obj* stack[STACK_SIZE]; 
    Obj* globals[GLOBAL_SIZE]; 
    int sp;
    int pc;
    bool running;

    Obj* firstObject;       
    int numObjects;         
    int maxObjects;         
} VM;

void run_vm_loop(VM* vm, int* program, int program_size);
void execute(VM* vm, int* program, int program_size);

void mark(Obj* root) {
    if (root == NULL || root->marked) return;

    Obj** worklist = malloc(sizeof(Obj*) * GC_WORKLIST_SIZE);
    if (!worklist) return;

    int top = 0;
    worklist[top++] = root;

    while (top > 0) {
        Obj* obj = worklist[--top];
        if (!obj || obj->marked) continue;

        obj->marked = true;

        if (obj->type == OBJ_PAIR) {
            if (obj->pair.left && !obj->pair.left->marked) worklist[top++] = obj->pair.left;
            if (obj->pair.right && !obj->pair.right->marked) worklist[top++] = obj->pair.right;
        }
    }
    free(worklist);
}



void markAll(VM* vm) {
    for (int i = 0; i <= vm->sp; i++) {
        mark(vm->stack[i]); 
    }
    for (int i = 0; i < GLOBAL_SIZE; i++) {
        mark(vm->globals[i]);
    }
}

void sweep(VM* vm) {
    Obj** object = &vm->firstObject;
    while (*object) {
        if (!(*object)->marked) {
            Obj* unreached = *object;
            *object = unreached->next;
            free(unreached);
            vm->numObjects--;
        } else {
            (*object)->marked = false; 
            object = &(*object)->next;
        }
    }
}

void gc(VM* vm) {
    int prevCount = vm->numObjects;
    markAll(vm);
    sweep(vm);
    vm->maxObjects = (vm->numObjects == 0) ? 1024 : vm->numObjects * 2;
    printf("GC: Collected %d objects. %d remaining.\n", prevCount - vm->numObjects, vm->numObjects);
}

void vm_free_all(VM* vm) {
    Obj* obj = vm->firstObject;
    while (obj) {
        Obj* next = obj->next;
        free(obj);
        obj = next;
    }
}

Obj* new_object(VM* vm, ObjType type) {
    if (vm->numObjects >= vm->maxObjects) gc(vm); 

    Obj* obj = malloc(sizeof(Obj));
    obj->type = type;
    obj->marked = false;
    obj->next = vm->firstObject;
    vm->firstObject = obj;
    vm->numObjects++;
    return obj;
}

void push_obj(VM* vm, Obj* obj) {
    if (vm->sp >= STACK_SIZE - 1) { fprintf(stderr, "Stack Overflow\n"); exit(1); }
    vm->stack[++vm->sp] = obj;
}

Obj* pop_obj(VM* vm) {
    if (vm->sp < 0) { fprintf(stderr, "Stack Underflow\n"); exit(1); }
    return vm->stack[vm->sp--];
}

void gc_vm_execute(VM *vm, int *program, int size) {
    while (vm->running && vm->pc < size) {
        int opcode = program[vm->pc++];
        switch (opcode) {
            case 0x01: { 
                Obj* obj = new_object(vm, OBJ_INT);
                obj->value = program[vm->pc++];
                push_obj(vm, obj);
                break;
            }
            case 0x02: pop_obj(vm); break; 
            case 0x03: 
                if (vm->sp >= 0) push_obj(vm, vm->stack[vm->sp]);
                break;
            case 0x10: { 
                Obj* b = pop_obj(vm);
                Obj* a = pop_obj(vm);
                Obj* res = new_object(vm, OBJ_INT);
                res->value = a->value + b->value;
                push_obj(vm, res);
                break;
            }
            case 0x23: gc(vm); break; 
            case 0x30: { 
                Obj* r = pop_obj(vm);
                Obj* l = pop_obj(vm);
                Obj* obj = new_object(vm, OBJ_PAIR);
                obj->pair.left = l;
                obj->pair.right = r;
                push_obj(vm, obj);
                break;
            }
            case 0x31: { 
                int addr = program[vm->pc++];
                push_obj(vm, vm->globals[addr]);
                break;
            }
            case 0x32: { 
                int addr = program[vm->pc++];
                vm->globals[addr] = pop_obj(vm);
                break;
            }
            case 0xFF: vm->running = false; break; 
        }
    }
}


void gc_execute_vm(int* program, int program_size) {
    
    VM* vm = malloc(sizeof(VM));
    memset(vm, 0, sizeof(VM)); 
    vm->sp = -1;
    vm->running = true;
    vm->maxObjects = 1024;

    run_vm_loop(vm, program, program_size); 
}

int gc_get_object_count(VM* vm) {
    (void)vm; 
    return 0; 
}
void run_vm_loop(VM* vm, int* program, int program_size) {
    gc_vm_execute(vm, program, program_size);
}