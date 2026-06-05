%{
#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "y.tab.h"

int line_num = 1;
%}

%option noyywrap

%%

"var"           { return VAR; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }

"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return MULT; }
"/"             { return DIV; }
"="             { return ASSIGN; }

"=="            { return EQ; }
"!="            { return NE; }
"<"             { return LT; }
">"             { return GT; }
"<="            { return LE; }
">="            { return GE; }

";"             { return SEMI; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
"("             { return LPAREN; }
")"             { return RPAREN; }

[0-9]+          { 
                    yylval.ival = atoi(yytext); 
                    return INTEGER; 
                }

[a-zA-Z_][a-zA-Z0-9_]* { 
                    yylval.sval = strdup(yytext); 
                    return IDENTIFIER; 
                }

\n              { line_num++; }
[ \t\r]+        { /* skip whitespace */ }

.               { printf("Unknown character: %s at line %d\n", yytext, line_num); }

%%