#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "../interface.h"
#include "y.tab.h"

extern int line_num;       
extern void yyrestart(FILE*); 

static int compile_error = 0; 

typedef struct { char name[64]; int depth; int index; } Local;
Local locals[256]; int local_count = 0; int scope_depth = 0;
typedef struct { char name[64]; } Global;
Global globals[256]; int global_count = 0;

int find_global(const char* name) {
    for(int i=0; i<global_count; i++) 
        if(strcmp(globals[i].name, name)==0) return i;
    return -1;
}

int declare_global(const char* name) {
    int idx = find_global(name);
    if (idx != -1) return idx;
    strcpy(globals[global_count].name, name); 
    return global_count++;
}

int resolve_local(const char* name) {
    for(int i=local_count-1; i>=0; i--) 
        if(locals[i].depth <= scope_depth && strcmp(locals[i].name, name)==0) return locals[i].index;
    return -1;
}

void add_local(const char* name) {
    strcpy(locals[local_count].name, name); locals[local_count].depth = scope_depth;
    locals[local_count].index = local_count; local_count++;
}

int code[4096]; int line_map[4096]; int c_idx = 0;
void emit(int op, int line) { code[c_idx] = op; line_map[c_idx++] = line; }

void compile_node(ASTNode* node) {
    if (compile_error) return;

    while(node) {
        switch(node->type) {
            case NODE_INT: emit(OP_PUSH, node->line); emit(node->data.val, node->line); break;
            
            case NODE_ID: {
                int lid = resolve_local(node->data.id_name);
                if(lid != -1) { 
                    emit(OP_LOAD_L, node->line); emit(lid, node->line); 
                } else { 
                    int gid = find_global(node->data.id_name);
                    if (gid == -1) {
                        printf("Compile Error: Undeclared variable '%s' used at line %d\n", node->data.id_name, node->line);
                        compile_error = 1; return;
                    }
                    emit(OP_LOAD_G, node->line); emit(gid, node->line); 
                }
                break;
            }
            case NODE_BINOP:
                compile_node(node->data.binop.left); compile_node(node->data.binop.right);
                if(compile_error) return;
                if(strcmp(node->data.binop.op,"+")==0) emit(OP_ADD, node->line);
                else if(strcmp(node->data.binop.op,"-")==0) emit(OP_SUB, node->line);
                else if(strcmp(node->data.binop.op,"*")==0) emit(OP_MUL, node->line);
                else if(strcmp(node->data.binop.op,"/")==0) emit(OP_DIV, node->line);
                else if(strcmp(node->data.binop.op,"<")==0) emit(OP_CMP, node->line);
                else if(strcmp(node->data.binop.op,">")==0) emit(OP_GT, node->line);
                else if(strcmp(node->data.binop.op,"==")==0) emit(OP_EQ, node->line);
                else if(strcmp(node->data.binop.op,"!=")==0) emit(OP_NEQ, node->line);
                break;
            case NODE_VAR_DECL:
                if(node->data.var_decl.init) compile_node(node->data.var_decl.init);
                else { emit(OP_PUSH, node->line); emit(0, node->line); }
                if(scope_depth==0) { emit(OP_STORE_G, node->line); emit(declare_global(node->data.var_decl.name), node->line); }
                else add_local(node->data.var_decl.name);
                break;
            case NODE_ASSIGN:
                compile_node(node->data.assign.expr);
                if(compile_error) return;
                int lid = resolve_local(node->data.assign.name);
                if(lid!=-1) { emit(OP_STORE_L, node->line); emit(lid, node->line); }
                else { 
                    int gid = find_global(node->data.assign.name);
                    if (gid == -1) {
                        printf("Compile Error: Cannot assign to undeclared variable '%s' at line %d\n", node->data.assign.name, node->line);
                        compile_error = 1; return;
                    }
                    emit(OP_STORE_G, node->line); emit(gid, node->line); 
                }
                break;
            case NODE_IF: {
                compile_node(node->data.if_stmt.cond); emit(OP_JZ, node->line); int jz=c_idx; emit(0, node->line);
                compile_node(node->data.if_stmt.then_branch);
                if(node->data.if_stmt.else_branch) {
                    emit(OP_JMP, node->line); int jmp=c_idx; emit(0, node->line);
                    code[jz]=c_idx; compile_node(node->data.if_stmt.else_branch); code[jmp]=c_idx;
                } else code[jz]=c_idx;
                break;
            }
            case NODE_WHILE: {
                int start=c_idx; compile_node(node->data.while_stmt.cond);
                emit(OP_JZ, node->line); int out=c_idx; emit(0, node->line);
                compile_node(node->data.while_stmt.body);
                emit(OP_JMP, node->line); emit(start, node->line);
                code[out]=c_idx;
                break;
            }
            case NODE_BLOCK: scope_depth++; compile_node(node->data.block.body); scope_depth--; break;
        }
        node = node->next;
    }
}

void print_ast(ASTNode* node, int depth) {
    while(node) {
        for(int i=0; i<depth; i++) printf("  ");
        switch(node->type) {
            case NODE_VAR_DECL: printf("VAR %s\n", node->data.var_decl.name); if(node->data.var_decl.init) print_ast(node->data.var_decl.init, depth+1); break;
            case NODE_ASSIGN: printf("ASSIGN %s\n", node->data.assign.name); print_ast(node->data.assign.expr, depth+1); break;
            case NODE_IF: printf("IF\n"); print_ast(node->data.if_stmt.cond, depth+1); printf("THEN\n"); print_ast(node->data.if_stmt.then_branch, depth+1); break;
            case NODE_WHILE: printf("WHILE\n"); print_ast(node->data.while_stmt.cond, depth+1); printf("DO\n"); print_ast(node->data.while_stmt.body, depth+1); break;
            case NODE_BLOCK: printf("BLOCK {\n"); print_ast(node->data.block.body, depth+1); printf("}\n"); break;
            case NODE_BINOP: printf("OP %s\n", node->data.binop.op); print_ast(node->data.binop.left, depth+1); print_ast(node->data.binop.right, depth+1); break;
            case NODE_INT: printf("INT %d\n", node->data.val); break;
            case NODE_ID: printf("ID %s\n", node->data.id_name); break;
        }
        node = node->next;
    }
}

extern FILE* yyin; extern ASTNode* root; extern int yyparse();

int compile_source(const char* filename, int** code_out, int* size_out, int** lines_out) {
    FILE* f = fopen(filename, "r");
    if (!f) return -1;
    
    line_num = 1;       
    yyrestart(f);       
    c_idx = 0;          
    global_count = 0;   
    local_count = 0;    
    compile_error = 0;  
    
    yyin = f;
    if(yyparse() == 0 && root) {
        printf("--- AST ---\n");
        print_ast(root, 0);
        printf("-----------\n");
        
        compile_node(root); 
        
        if (compile_error) {
            free_ast(root); root=NULL; fclose(f); return 1;
        }

        emit(OP_HALT, 0);
        *code_out=malloc(sizeof(int)*c_idx); memcpy(*code_out, code, sizeof(int)*c_idx);
        *lines_out=malloc(sizeof(int)*c_idx); memcpy(*lines_out, line_map, sizeof(int)*c_idx);
        *size_out=c_idx; free_ast(root); root=NULL; fclose(f); return 0;
    }
    fclose(f); return 1;
}