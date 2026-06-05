#include <stdlib.h>
#include <stdio.h>
#include "ast.h"

ASTNode* create_node(NodeType type, int line) {
    ASTNode* node = malloc(sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    node->type = type;
    node->line = line;
    node->next = NULL;
    return node;
}