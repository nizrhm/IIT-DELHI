%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
extern int yylex();
extern int line_num;
extern char* yytext; 
void yyerror(const char* s) { 
    fprintf(stderr, "Syntax Error line %d: %s at '%s'\n", line_num, s, yytext); 
}
ASTNode* root = NULL;
%}

%define parse.error verbose

%union { int ival; char* sval; struct ASTNode* node; }

%token <ival> INTEGER
%token <sval> IDENTIFIER
%token VAR IF ELSE WHILE PLUS MINUS MULT DIV ASSIGN LT GT EQ NEQ SEMI LBRACE RBRACE LPAREN RPAREN

%type <node> program statement_list statement var_decl assignment if_stmt while_stmt block expression

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%left LT GT EQ NEQ
%left PLUS MINUS
%left MULT DIV

%%

program: statement_list { root = $1; };

statement_list: statement { $$ = $1; } 
              | statement_list statement { ASTNode* c = $1; while(c->next) c = c->next; c->next = $2; $$ = $1; };

statement: var_decl { $$=$1; } 
         | assignment { $$=$1; } 
         | if_stmt { $$=$1; } 
         | while_stmt { $$=$1; } 
         | block { $$=$1; };

block: LBRACE statement_list RBRACE { $$ = create_node(NODE_BLOCK, line_num); $$->data.block.body = $2; };

var_decl: VAR IDENTIFIER ASSIGN expression SEMI { $$ = create_node(NODE_VAR_DECL, line_num); $$->data.var_decl.name = $2; $$->data.var_decl.init = $4; }
        | VAR IDENTIFIER SEMI { $$ = create_node(NODE_VAR_DECL, line_num); $$->data.var_decl.name = $2; $$->data.var_decl.init = NULL; };

assignment: IDENTIFIER ASSIGN expression SEMI { $$ = create_node(NODE_ASSIGN, line_num); $$->data.assign.name = $1; $$->data.assign.expr = $3; };

if_stmt: IF LPAREN expression RPAREN statement %prec LOWER_THAN_ELSE { 
             $$ = create_node(NODE_IF, line_num); $$->data.if_stmt.cond = $3; 
             $$->data.if_stmt.then_branch = $5; $$->data.if_stmt.else_branch = NULL; 
         }
       | IF LPAREN expression RPAREN statement ELSE statement {
             $$ = create_node(NODE_IF, line_num); $$->data.if_stmt.cond = $3; 
             $$->data.if_stmt.then_branch = $5; $$->data.if_stmt.else_branch = $7; 
         };

while_stmt: WHILE LPAREN expression RPAREN statement { $$ = create_node(NODE_WHILE, line_num); $$->data.while_stmt.cond = $3; $$->data.while_stmt.body = $5; };

expression: INTEGER { $$ = create_node(NODE_INT, line_num); $$->data.val = $1; }
          | IDENTIFIER { $$ = create_node(NODE_ID, line_num); $$->data.id_name = $1; }
          | LPAREN expression RPAREN { $$ = $2; }
          | expression PLUS expression { $$ = create_node(NODE_BINOP, line_num); $$->data.binop.op="+"; $$->data.binop.left=$1; $$->data.binop.right=$3; }
          | expression MINUS expression { $$ = create_node(NODE_BINOP, line_num); $$->data.binop.op="-"; $$->data.binop.left=$1; $$->data.binop.right=$3; }
          | expression MULT expression { $$ = create_node(NODE_BINOP, line_num); $$->data.binop.op="*"; $$->data.binop.left=$1; $$->data.binop.right=$3; }
          | expression DIV expression { $$ = create_node(NODE_BINOP, line_num); $$->data.binop.op="/"; $$->data.binop.left=$1; $$->data.binop.right=$3; }
          | expression LT expression { $$ = create_node(NODE_BINOP, line_num); $$->data.binop.op="<"; $$->data.binop.left=$1; $$->data.binop.right=$3; }
          | expression GT expression { $$ = create_node(NODE_BINOP, line_num); $$->data.binop.op=">"; $$->data.binop.left=$1; $$->data.binop.right=$3; }
          | expression EQ expression { $$ = create_node(NODE_BINOP, line_num); $$->data.binop.op="=="; $$->data.binop.left=$1; $$->data.binop.right=$3; }
          | expression NEQ expression { $$ = create_node(NODE_BINOP, line_num); $$->data.binop.op="!="; $$->data.binop.left=$1; $$->data.binop.right=$3; };
%%