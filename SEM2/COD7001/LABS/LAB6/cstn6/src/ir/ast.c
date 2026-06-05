#include <stdlib.h>
#include <string.h>
#include "ast.h"

ASTNode* create_node(NodeType type, int line) {
    ASTNode* node = malloc(sizeof(ASTNode));
    memset(node, 0, sizeof(ASTNode)); node->type = type; node->line = line;
    return node;
}
void free_ast(ASTNode* node) {
    if (!node) return;
    free_ast(node->next);
    switch (node->type) {
        case NODE_VAR_DECL: free(node->data.var_decl.name); free_ast(node->data.var_decl.init); break;
        case NODE_ASSIGN: free(node->data.assign.name); free_ast(node->data.assign.expr); break;
        case NODE_IF: free_ast(node->data.if_stmt.cond); free_ast(node->data.if_stmt.then_branch); free_ast(node->data.if_stmt.else_branch); break;
        case NODE_WHILE: free_ast(node->data.while_stmt.cond); free_ast(node->data.while_stmt.body); break;
        case NODE_BLOCK: free_ast(node->data.block.body); break;
        case NODE_BINOP: free_ast(node->data.binop.left); free_ast(node->data.binop.right); break;
        case NODE_ID: free(node->data.id_name); break;
        default: break;
    }
    free(node);
}