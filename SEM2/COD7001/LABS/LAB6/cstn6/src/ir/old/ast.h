#ifndef AST_H
#define AST_H

typedef enum {
    NODE_VAR_DECL, NODE_ASSIGN, NODE_BINOP,
    NODE_INT, NODE_ID, NODE_IF, NODE_WHILE, NODE_BLOCK
} NodeType;

typedef struct ASTNode {
    NodeType type;
    int line;  // CRITICAL for Lab 6 Debugger
    union {
        struct { char* name; struct ASTNode* init; } var_decl;
        struct { char* name; struct ASTNode* expr; } assign;
        struct { char* op; struct ASTNode* left; struct ASTNode* right; } binop;
        struct { struct ASTNode* cond; struct ASTNode* then_branch; struct ASTNode* else_branch; } if_stmt;
        struct { struct ASTNode* cond; struct ASTNode* body; } while_stmt;
        struct { struct ASTNode* body; } block;
        int val;
        char* id_name;
    } data;
    struct ASTNode* next; 
} ASTNode;

ASTNode* create_node(NodeType type, int line);
void free_ast(ASTNode* node);

#endif