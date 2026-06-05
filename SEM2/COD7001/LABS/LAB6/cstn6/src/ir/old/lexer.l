%{
#include "y.tab.h"
#include <string.h>

int line_num = 1;
%}

%%

"var"       { return VAR; }
"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }

[a-zA-Z_][a-zA-Z0-9_]* { 
    yylval.sval = strdup(yytext); 
    return IDENTIFIER; 
}

[0-9]+ { 
    yylval.ival = atoi(yytext); 
    return INTEGER; 
}

"+"     { return PLUS; }
"-"     { return MINUS; }
"*"     { return MULT; }
"/"     { return DIV; }
"="     { return ASSIGN; }
"<"     { return LT; }
">"     { return GT; }
"=="    { return EQ; }

";"     { return SEMI; }
"{"     { return LBRACE; }
"}"     { return RBRACE; }
"("     { return LPAREN; }
")"     { return RPAREN; }

[\n]    { line_num++; } /* CRITICAL: Debugger needs this! */
[ \t]   { /* Ignore whitespace */ }

.       { printf("Unknown character: %s\n", yytext); return 0; }

%%

int yywrap() { return 1; }