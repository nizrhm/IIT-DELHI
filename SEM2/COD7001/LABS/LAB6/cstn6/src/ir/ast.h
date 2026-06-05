#ifndef AST_H
#define AST_H
typedef enum { NODE_VAR_DECL, NODE_ASSIGN, NODE_IF, NODE_WHILE, NODE_BLOCK, NODE_BINOP, NODE_INT, NODE_ID } NodeType;
typedef struct ASTNode {
    NodeType type; int line; struct ASTNode* next; 
    union {
        struct { char* name; struct ASTNode* init; } var_decl;
        struct { char* name; struct ASTNode* expr; } assign;
        struct { struct ASTNode *cond, *then_branch, *else_branch; } if_stmt;
        struct { struct ASTNode *cond, *body; } while_stmt;
        struct { struct ASTNode *body; } block;
        struct { char* op; struct ASTNode *left, *right; } binop;
        int val; char* id_name;
    } data;
} ASTNode;
ASTNode* create_node(NodeType type, int line);
void free_ast(ASTNode* node);
#endif