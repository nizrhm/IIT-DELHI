#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../interface.h"
#include "ast.h"      // FROM LAB 3
#include "y.tab.h"    // FROM LAB 3 (Defines tokens like PLUS, MINUS)

// External Globals from Lab 3 Parser
extern int yyparse();
extern FILE* yyin;
extern ASTNode* root; // The top of the tree

// --- SYMBOL TABLE (Simple Version) ---
// Maps variable names "x", "y" to global memory indices 0, 1...
char* sym_table[256];
int sym_count = 0;

int get_symbol_addr(char* name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i], name) == 0) return i;
    }
    // New variable? Add it.
    sym_table[sym_count] = strdup(name);
    return sym_count++;
}

// --- CODE BUFFER ---
typedef struct {
    int* code;
    int* lines;
    int count;
    int capacity;
} CodeBuffer;

void emit(CodeBuffer* b, int op, int line) {
    if (b->count >= b->capacity) {
        b->capacity *= 2;
        b->code = realloc(b->code, b->capacity * sizeof(int));
        b->lines = realloc(b->lines, b->capacity * sizeof(int));
    }
    b->code[b->count] = op;
    b->lines[b->count] = line;
    b->count++;
}

// Placeholder for backpatching jumps
int emit_placeholder(CodeBuffer* b) {
    emit(b, 0, 0); 
    return b->count - 1;
}

void patch(CodeBuffer* b, int addr, int target) {
    b->code[addr] = target;
}

// --- RECURSIVE CODE GENERATOR ---
void gen(ASTNode* node, CodeBuffer* b) {
    if (!node) return;

    // Based on AST Node Type (Lab 3)
    switch (node->type) {
        case NODE_INT:
            emit(b, 0x01, 0);      // PUSH
            emit(b, node->data.val, 0);
            break;

        case NODE_ID: {
            int addr = get_symbol_addr(node->data.id_name);
            emit(b, 0x31, 0);      // LOAD (Lab 5 Opcode)
            emit(b, addr, 0);
            break;
        }

        case NODE_ASSIGN: {
            // 1. Generate value expr
            gen(node->data.assign.expr, b);
            // 2. Store to var
            int addr = get_symbol_addr(node->data.assign.name);
            emit(b, 0x32, 0);      // STORE (Lab 5 Opcode)
            emit(b, addr, 0);
            break;
        }

        case NODE_BINOP:
            gen(node->data.binop.left, b);
            gen(node->data.binop.right, b);
            
            // Map string op to Opcode
            char* op = node->data.binop.op;
            if (strcmp(op, "+") == 0) emit(b, 0x10, 0); // ADD
            else if (strcmp(op, "-") == 0) emit(b, 0x11, 0); // SUB
            else if (strcmp(op, "*") == 0) emit(b, 0x12, 0); // MUL
            else if (strcmp(op, "/") == 0) emit(b, 0x13, 0); // DIV
            else if (strcmp(op, "<") == 0) emit(b, 0x14, 0); // CMP
            // ... add others ...
            break;

        case NODE_IF: {
            /* Structure:
               [Condition Code]
               JZ else_label
               [Then Code]
               JMP end_label
               else_label: [Else Code]
               end_label:
            */
            gen(node->data.if_stmt.cond, b);
            
            emit(b, 0x21, 0); // JZ
            int jump_else_idx = emit_placeholder(b);

            gen(node->data.if_stmt.then_branch, b);

            if (node->data.if_stmt.else_branch) {
                emit(b, 0x20, 0); // JMP
                int jump_end_idx = emit_placeholder(b);
                
                // Patch JZ to jump here (start of else)
                patch(b, jump_else_idx, b->count);
                
                gen(node->data.if_stmt.else_branch, b);
                
                // Patch JMP to jump here (end)
                patch(b, jump_end_idx, b->count);
            } else {
                // No else? Patch JZ to jump to end
                patch(b, jump_else_idx, b->count);
            }
            break;
        }

        case NODE_WHILE: {
            /*
               start_label:
               [Condition]
               JZ end_label
               [Body]
               JMP start_label
               end_label:
            */
            int start_addr = b->count;
            
            gen(node->data.while_stmt.cond, b);
            
            emit(b, 0x21, 0); // JZ
            int jump_end_idx = emit_placeholder(b);
            
            gen(node->data.while_stmt.body, b);
            
            emit(b, 0x20, 0); // JMP back
            emit(b, start_addr, 0);
            
            patch(b, jump_end_idx, b->count); // Patch break
            break;
        }

        case NODE_BLOCK:
            // Just traverse the list
            gen(node->data.block.body, b);
            break;
        
        case NODE_VAR_DECL:
            // Handle initialization if present
            if (node->data.var_decl.init) {
                gen(node->data.var_decl.init, b);
                int addr = get_symbol_addr(node->data.var_decl.name);
                emit(b, 0x32, 0); // STORE
                emit(b, addr, 0);
            }
            break;
    }

    // Traverse Siblings (Linked List of Statements)
    if (node->next) gen(node->next, b);
}

// --- MAIN EXPORTED FUNCTION ---
int compile_source(const char* filename, int** code_out, int* size_out, int** lines_out) {
    FILE* f = fopen(filename, "r");
    if (!f) return -1;

    // Reset Lab 3 Global State
    root = NULL;
    yyin = f;
    
    // Parse
    if (yyparse() != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);

    if (!root) return -1; // Empty program

    // Init Buffer
    CodeBuffer buf;
    buf.capacity = 1024;
    buf.count = 0;
    buf.code = malloc(sizeof(int) * buf.capacity);
    buf.lines = malloc(sizeof(int) * buf.capacity);

    // Generate
    gen(root, &buf);
    
    // Always append HALT
    emit(&buf, 0xFF, 0);

    *code_out = buf.code;
    *lines_out = buf.lines;
    *size_out = buf.count;
    
    return 0;
}