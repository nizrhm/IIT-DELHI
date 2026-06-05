#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "y.tab.h"

extern int yyparse();
extern int line_num;
extern ASTNode* root; 


ASTNode* ir_parse_to_ast(char* filename) {
    printf("Starting Parser...\n");

    if (yyparse() == 0) {
        printf("Parsing Successful!\n");
        printf("--- Abstract Syntax Tree ---\n");
        
        if (root != NULL) {
            print_ast(root, 0);
        } else {
            printf("Empty AST (No statements found).\n");
        }
    } else {
        printf("Parsing Failed.\n");
        return 1;
    }

    return 0;
}

void yyerror(const char* s) {
    fprintf(stderr, "Syntax Error at line %d: %s\n", line_num, s);
}