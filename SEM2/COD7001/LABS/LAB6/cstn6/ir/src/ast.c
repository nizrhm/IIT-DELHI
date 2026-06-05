#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

ASTNode* create_node(NodeType type) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    node->type = type;
    node->next = NULL; 
    return node;
}

void print_ast(ASTNode* node, int indent) {
    while (node != NULL) {
        for (int i = 0; i < indent; i++) printf("  ");

        switch (node->type) {
            case NODE_VAR_DECL:
                printf("VAR_DECL: %s\n", node->data.var_decl.name);
                if (node->data.var_decl.init) {
                    print_ast(node->data.var_decl.init, indent + 1);
                }
                break;

            case NODE_ASSIGN:
                printf("ASSIGN: %s\n", node->data.assign.name);
                print_ast(node->data.assign.expr, indent + 1);
                break;

            case NODE_BINOP:
                printf("BINOP: %s\n", node->data.binop.op);
                print_ast(node->data.binop.left, indent + 1);
                print_ast(node->data.binop.right, indent + 1);
                break;

            case NODE_INT:
                printf("INT: %d\n", node->data.val);
                break;

            case NODE_ID:
                printf("ID: %s\n", node->data.id_name);
                break;

            case NODE_IF:
                printf("IF_STATEMENT\n");
                for (int i = 0; i <= indent; i++) printf("  ");
                printf("CONDITION:\n");
                print_ast(node->data.if_stmt.cond, indent + 2);
                
                for (int i = 0; i <= indent; i++) printf("  ");
                printf("THEN:\n");
                print_ast(node->data.if_stmt.then_branch, indent + 2);
                
                if (node->data.if_stmt.else_branch) {
                    for (int i = 0; i <= indent; i++) printf("  ");
                    printf("ELSE:\n");
                    print_ast(node->data.if_stmt.else_branch, indent + 2);
                }
                break;

            case NODE_WHILE:
                printf("WHILE_LOOP\n");
                for (int i = 0; i <= indent; i++) printf("  ");
                printf("CONDITION:\n");
                print_ast(node->data.while_stmt.cond, indent + 2);
                
                for (int i = 0; i <= indent; i++) printf("  ");
                printf("BODY:\n");
                print_ast(node->data.while_stmt.body, indent + 2);
                break;

            case NODE_BLOCK:
                printf("BLOCK\n");
                print_ast(node->data.block.body, indent + 1);
                break;

            default:
                printf("Unknown Node Type\n");
        }
        node = node->next;
    }
}

ASTNode* ir_parse_to_ast(char* filename) {
    extern FILE* yyin;
    extern int yyparse();
    extern ASTNode* root; 

    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    
    yyin = file;
    if (yyparse() == 0) {
        fclose(file);
        return root;
    }
    
    fclose(file);
    return NULL;
}

void yyerror(const char* s) {
    fprintf(stderr, "Parse error: %s\n", s);
}