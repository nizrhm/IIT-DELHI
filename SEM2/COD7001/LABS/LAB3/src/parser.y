%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yylex();
extern int line_num;
void yyerror(const char* s);

ASTNode* root = NULL;
%}

%union {
    int ival;
    char* sval;
    struct ASTNode* node;
}

%token <ival> INTEGER
%token <sval> IDENTIFIER
%token VAR IF ELSE WHILE
%token PLUS MINUS MULT DIV ASSIGN
%token EQ NE LT GT LE GE
%token SEMI LBRACE RBRACE LPAREN RPAREN

%type <node> program statement_list statement variable_decl assignment if_statement while_statement block expression

/* Precedence Rules */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%left EQ NE LT GT LE GE
%left PLUS MINUS
%left MULT DIV

%%

program:
    statement_list { root = $1; }
    ;

statement_list:
    statement { $$ = $1; }
    | statement_list statement {
        ASTNode* curr = $1;
        while (curr->next != NULL) curr = curr->next;
        curr->next = $2;
        $$ = $1;
    }
    ;

statement:
    variable_decl { $$ = $1; }
    | assignment { $$ = $1; }
    | if_statement { $$ = $1; }
    | while_statement { $$ = $1; }
    | block { $$ = $1; }
    ;

block:
    LBRACE statement_list RBRACE {
        $$ = create_node(NODE_BLOCK);
        $$->data.block.body = $2;
    }
    ;

variable_decl:
    VAR IDENTIFIER SEMI {
        $$ = create_node(NODE_VAR_DECL);
        $$->data.var_decl.name = $2;
        $$->data.var_decl.init = NULL;
    }
    | VAR IDENTIFIER ASSIGN expression SEMI {
        $$ = create_node(NODE_VAR_DECL);
        $$->data.var_decl.name = $2;
        $$->data.var_decl.init = $4;
    }
    ;

assignment:
    IDENTIFIER ASSIGN expression SEMI {
        $$ = create_node(NODE_ASSIGN);
        $$->data.assign.name = $1;
        $$->data.assign.expr = $3;
    }
    ;

if_statement:
    IF LPAREN expression RPAREN statement %prec LOWER_THAN_ELSE {
        $$ = create_node(NODE_IF);
        $$->data.if_stmt.cond = $3;
        $$->data.if_stmt.then_branch = $5;
        $$->data.if_stmt.else_branch = NULL;
    }
    | IF LPAREN expression RPAREN statement ELSE statement {
        $$ = create_node(NODE_IF);
        $$->data.if_stmt.cond = $3;
        $$->data.if_stmt.then_branch = $5;
        $$->data.if_stmt.else_branch = $7;
    }
    ;

while_statement:
    WHILE LPAREN expression RPAREN statement {
        $$ = create_node(NODE_WHILE);
        $$->data.while_stmt.cond = $3;
        $$->data.while_stmt.body = $5;
    }
    ;

expression:
    INTEGER {
        $$ = create_node(NODE_INT);
        $$->data.val = $1;
    }
    | IDENTIFIER {
        $$ = create_node(NODE_ID);
        $$->data.id_name = $1;
    }
    | expression PLUS expression  { $$ = create_node(NODE_BINOP); $$->data.binop.op = "+";  $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression MINUS expression { $$ = create_node(NODE_BINOP); $$->data.binop.op = "-";  $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression MULT expression  { $$ = create_node(NODE_BINOP); $$->data.binop.op = "*";  $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression DIV expression   { $$ = create_node(NODE_BINOP); $$->data.binop.op = "/";  $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression EQ expression    { $$ = create_node(NODE_BINOP); $$->data.binop.op = "=="; $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression NE expression    { $$ = create_node(NODE_BINOP); $$->data.binop.op = "!="; $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression LT expression    { $$ = create_node(NODE_BINOP); $$->data.binop.op = "<";  $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression GT expression    { $$ = create_node(NODE_BINOP); $$->data.binop.op = ">";  $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression LE expression    { $$ = create_node(NODE_BINOP); $$->data.binop.op = "<="; $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | expression GE expression    { $$ = create_node(NODE_BINOP); $$->data.binop.op = ">="; $$->data.binop.left = $1; $$->data.binop.right = $3; }
    | LPAREN expression RPAREN    { $$ = $2; }
    ;

%%