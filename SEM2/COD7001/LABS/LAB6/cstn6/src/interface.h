#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdbool.h>

// --- OPCODES ---
#define OP_HALT     0
#define OP_PUSH     1
#define OP_ADD      2
#define OP_SUB      3
#define OP_MUL      4
#define OP_DIV      5
#define OP_LOAD_L   6  // Load Local
#define OP_STORE_L  7  // Store Local
#define OP_LOAD_G   8  // Load Global
#define OP_STORE_G  9  // Store Global
#define OP_JMP      10
#define OP_JZ       11
#define OP_CMP      12 // <
#define OP_GT       13 // >
#define OP_EQ       14 // ==
#define OP_NEQ      15 // !=

// --- PROCESS STATES ---
#define PROC_FREE     0
#define PROC_STOPPED  1
#define PROC_RUNNING  2

// --- VM & MEMORY CONSTANTS ---
#define MAX_OBJECTS 4096
#define MAX_STACK   256

// --- OBJECT SYSTEM (For Garbage Collection) ---
typedef enum { 
    OBJ_FREE, 
    OBJ_INT 
} ObjectType;

typedef struct Object {
    ObjectType type;
    int marked;          // GC Mark Bit
    struct Object* next; // For tracking allocation list
    int value;           // Payload (Integer)
} Object;

// --- VIRTUAL MACHINE STATE ---
typedef struct VM {
    int* program;
    int program_size;
    int* line_map;     // Map PC -> Source Line
    
    int pc;            // Program Counter
    int sp;            // Stack Pointer
    int fp;            // Frame Pointer
    int csp;           // Call Stack Pointer
    
    bool running;
    bool paused;
    bool breakpoints[4096];

    // Stack now holds POINTERS to Objects (References)
    Object* stack[MAX_STACK]; 
    
    // Managed Heap
    Object heap[MAX_OBJECTS];
    Object* objects;   // Head of active object list (for Sweep)
    int numObjects;    // Allocation counter
} VM;

// --- PROCESS TABLE ENTRY ---
typedef struct {
    int pid;
    int state;
    VM* vm;
} Process;

// --- FUNCTION PROTOTYPES ---

// Compiler
int compile_source(const char* filename, int** code_out, int* size_out, int** lines_out);

// VM Management
VM* vm_create(int* code, int code_size, int* lines);
void vm_run(VM* vm);
void vm_step(VM* vm);

// Memory Management
void vm_gc(VM* vm);
int vm_get_heap_count(VM* vm);

#endif