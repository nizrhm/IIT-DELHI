%{
#include "y.tab.h"
#include <string.h>
int line_num = 1;
%}

%option noinput nounput
%x BLOCK_COMMENT

%%

"//"[^\n]* { /* Ignore single-line comment */ }

"/*"        { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"*/" { BEGIN(INITIAL); }
<BLOCK_COMMENT>\n   { line_num++; }
<BLOCK_COMMENT>.    { /* Ignore content inside */ }

"var"       { return VAR; }
"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }

[a-zA-Z_][a-zA-Z0-9_]* { yylval.sval = strdup(yytext); return IDENTIFIER; }
[0-9]+                 { yylval.ival = atoi(yytext); return INTEGER; }

"+"         { return PLUS; }
"-"         { return MINUS; }
"*"         { return MULT; }
"/"         { return DIV; }
"="         { return ASSIGN; }
"<"         { return LT; }
">"         { return GT; }
"=="        { return EQ; }
"!="        { return NEQ; }

";"         { return SEMI; }
"{"         { return LBRACE; }
"}"         { return RBRACE; }
"("         { return LPAREN; }
")"         { return RPAREN; }

[\n]        { line_num++; }
[ \t\r]     { /* Ignore whitespace and CR */ }
.           { printf("Unknown char: %s\n", yytext); }

%%

int yywrap() { return 1; }