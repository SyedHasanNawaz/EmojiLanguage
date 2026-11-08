%{
#include "ast.h"
#include "parser.h"
#include <string>
#include <cstdlib>

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yylloc.last_column + 1; \
    yylloc.last_column += yyleng;

%}

%option noyywrap
%option yylineno

%%

"🔢"                   { return TOKEN_DECL; }
"📥"                   { return TOKEN_ASSIGN; }
"🤔"                   { return TOKEN_IF; }
"😢"                   { return TOKEN_ELSE; }
"🔁"                   { return TOKEN_WHILE; }
("⚙️"|"⚙")             { return TOKEN_FUNCDEF; }
("↩️"|"↩")             { return TOKEN_RETURN; }
"📢"                   { return TOKEN_PRINT; }
"📦"                   { return TOKEN_BOX; }
"🔍"                   { return TOKEN_INDEX; }
"📏"                   { return TOKEN_LEN; }
("✂️"|"✂")             { return TOKEN_SLICE; }

"➕"                   { return TOKEN_PLUS; }
"➖"                   { return TOKEN_MINUS; }
("✖️"|"✖")             { return TOKEN_MUL; }
"➗"                   { return TOKEN_DIV; }
("⚖️"|"⚖")             { return TOKEN_EQ; }

"+"                    { return TOKEN_PLUS; }
"-"                    { return TOKEN_MINUS; }
"*"                    { return TOKEN_MUL; }
"/"                    { return TOKEN_DIV; }
"%"                    { return TOKEN_MOD; }

"<="                   { return TOKEN_LE; }
">="                   { return TOKEN_GE; }
"=="                   { return TOKEN_EQ; }
"!="                   { return TOKEN_NE; }
"<"                    { return TOKEN_LT; }
">"                    { return TOKEN_GT; }

"&&"                   { return TOKEN_AND; }
"||"                   { return TOKEN_OR; }

"="                    { return '='; }
"("                    { return '('; }
")"                    { return ')'; }
"["                    { return '['; }
"]"                    { return ']'; }
"{"                    { return '{'; }
"}"                    { return '}'; }
","                    { return ','; }
";"                    { return ';'; }

"💬"[^\r\n]*           { /* ignore single-line comments */ }

[0-9]+ {
    yylval.num = std::stoll(yytext);
    return TOKEN_INT;
}

[a-zA-Z_][a-zA-Z0-9_]* {
    yylval.str = new std::string(yytext);
    return TOKEN_ID;
}

\n {
    yylloc.last_column = 0;
}

[ \t\r]+ {
    /* ignore whitespace */
}

. {
    return yytext[0];
}

%%
