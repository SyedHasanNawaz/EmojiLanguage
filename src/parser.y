%expect 12
%{
#include "ast.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>

int yylex();
void yyerror(const char* s);
extern int yylineno;
extern std::unique_ptr<Program> g_program;
%}

%locations

%union {
    long long num;
    std::string* str;
    Expr* expr;
    Stmt* stmt;
    BlockStmt* block;
    std::vector<std::unique_ptr<Stmt>>* stmt_list;
    std::vector<std::unique_ptr<Expr>>* expr_list;
    std::vector<std::string>* str_list;
}


%token TOKEN_DECL TOKEN_ASSIGN TOKEN_IF TOKEN_ELSE TOKEN_WHILE
%token TOKEN_FUNCDEF TOKEN_RETURN TOKEN_PRINT TOKEN_BOX TOKEN_INDEX
%token TOKEN_LEN TOKEN_SLICE
%token TOKEN_PLUS TOKEN_MINUS TOKEN_MUL TOKEN_DIV TOKEN_MOD
%token TOKEN_LT TOKEN_GT TOKEN_LE TOKEN_GE TOKEN_EQ TOKEN_NE
%token TOKEN_AND TOKEN_OR
%token <num> TOKEN_INT
%token <str> TOKEN_ID

%type <stmt_list> program stmts
%type <stmt> stmt decl assign ifstmt whilestmt funcdef retstmt printstmt
%type <block> block
%type <str_list> params param_list
%type <expr> expr logical_or logical_and equality relational additive multiplicative unary primary
%type <expr_list> args arg_list

%%

program   : stmts
            {
                g_program = std::make_unique<Program>(std::move(*$1), @$.first_line);
                delete $1;
            }
          ;

stmts     : stmt
            {
                $$ = new std::vector<std::unique_ptr<Stmt>>();
                if ($1) {
                    $$->push_back(std::unique_ptr<Stmt>($1));
                }
            }
          | stmts stmt
            {
                if ($2) {
                    $1->push_back(std::unique_ptr<Stmt>($2));
                }
                $$ = $1;
            }
          ;

stmt      : decl      { $$ = $1; }
          | assign    { $$ = $1; }
          | ifstmt    { $$ = $1; }
          | whilestmt { $$ = $1; }
          | funcdef   { $$ = $1; }
          | retstmt   { $$ = $1; }
          | printstmt { $$ = $1; }
          | expr      { $$ = new ExprStmt(std::unique_ptr<Expr>($1), @$.first_line); }
          | ';'       { $$ = nullptr; }
          ;

decl      : TOKEN_DECL TOKEN_ID '=' expr
            {
                $$ = new DeclStmt(*$2, std::unique_ptr<Expr>($4), @$.first_line);
                delete $2;
            }
          ;

assign    : TOKEN_ASSIGN TOKEN_ID '=' expr
            {
                $$ = new AssignStmt(*$2, std::unique_ptr<Expr>($4), true, @$.first_line);
                delete $2;
            }
          | TOKEN_ID '=' expr
            {
                $$ = new AssignStmt(*$1, std::unique_ptr<Expr>($3), false, @$.first_line);
                delete $1;
            }
          ;

ifstmt    : TOKEN_IF expr block
            {
                $$ = new IfStmt(std::unique_ptr<Expr>($2), std::unique_ptr<BlockStmt>($3), nullptr, @$.first_line);
            }
          | TOKEN_IF expr block TOKEN_ELSE block
            {
                $$ = new IfStmt(std::unique_ptr<Expr>($2), std::unique_ptr<BlockStmt>($3), std::unique_ptr<BlockStmt>($5), @$.first_line);
            }
          ;

whilestmt : TOKEN_WHILE expr block
            {
                $$ = new WhileStmt(std::unique_ptr<Expr>($2), std::unique_ptr<BlockStmt>($3), @$.first_line);
            }
          ;

funcdef   : TOKEN_FUNCDEF TOKEN_ID '(' params ')' block
            {
                $$ = new FuncDefStmt(*$2, std::move(*$4), std::unique_ptr<BlockStmt>($6), @$.first_line);
                delete $2;
                delete $4;
            }
          ;

retstmt   : TOKEN_RETURN expr
            {
                $$ = new ReturnStmt(std::unique_ptr<Expr>($2), @$.first_line);
            }
          ;

printstmt : TOKEN_PRINT expr
            {
                $$ = new PrintStmt(std::unique_ptr<Expr>($2), @$.first_line);
            }
          ;

block     : '{' stmts '}'
            {
                $$ = new BlockStmt(std::move(*$2), @$.first_line);
                delete $2;
            }
          | '{' '}'
            {
                $$ = new BlockStmt({}, @$.first_line);
            }
          ;

params    : /* empty */
            {
                $$ = new std::vector<std::string>();
            }
          | param_list
            {
                $$ = $1;
            }
          ;

param_list: TOKEN_ID
            {
                $$ = new std::vector<std::string>();
                $$->push_back(*$1);
                delete $1;
            }
          | param_list ',' TOKEN_ID
            {
                $1->push_back(*$3);
                delete $3;
                $$ = $1;
            }
          ;

expr      : logical_or
            {
                $$ = $1;
            }
          ;

logical_or: logical_and
            {
                $$ = $1;
            }
          | logical_or TOKEN_OR logical_and
            {
                $$ = new BinOpExpr("||", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          ;

logical_and: equality
             {
                 $$ = $1;
             }
           | logical_and TOKEN_AND equality
             {
                 $$ = new BinOpExpr("&&", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
             }
           ;

equality  : relational
            {
                $$ = $1;
            }
          | equality TOKEN_EQ relational
            {
                $$ = new BinOpExpr("==", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          | equality TOKEN_NE relational
            {
                $$ = new BinOpExpr("!=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          ;

relational: additive
            {
                $$ = $1;
            }
          | relational TOKEN_LT additive
            {
                $$ = new BinOpExpr("<", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          | relational TOKEN_GT additive
            {
                $$ = new BinOpExpr(">", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          | relational TOKEN_LE additive
            {
                $$ = new BinOpExpr("<=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          | relational TOKEN_GE additive
            {
                $$ = new BinOpExpr(">=", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          ;

additive  : multiplicative
            {
                $$ = $1;
            }
          | additive '+' multiplicative
            {
                $$ = new BinOpExpr("+", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          | additive TOKEN_PLUS multiplicative
            {
                $$ = new BinOpExpr("+", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          | additive '-' multiplicative
            {
                $$ = new BinOpExpr("-", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          | additive TOKEN_MINUS multiplicative
            {
                $$ = new BinOpExpr("-", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
            }
          ;

multiplicative: unary
                {
                    $$ = $1;
                }
              | multiplicative '*' unary
                {
                    $$ = new BinOpExpr("*", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
                }
              | multiplicative TOKEN_MUL unary
                {
                    $$ = new BinOpExpr("*", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
                }
              | multiplicative '/' unary
                {
                    $$ = new BinOpExpr("/", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
                }
              | multiplicative TOKEN_DIV unary
                {
                    $$ = new BinOpExpr("/", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
                }
              | multiplicative '%' unary
                {
                    $$ = new BinOpExpr("%", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
                }
              | multiplicative TOKEN_MOD unary
                {
                    $$ = new BinOpExpr("%", std::unique_ptr<Expr>($1), std::unique_ptr<Expr>($3), @$.first_line);
                }
              ;

unary     : '-' unary
            {
                $$ = new BinOpExpr("-", std::make_unique<NumExpr>(0, @$.first_line), std::unique_ptr<Expr>($2), @$.first_line);
            }
          | TOKEN_MINUS unary
            {
                $$ = new BinOpExpr("-", std::make_unique<NumExpr>(0, @$.first_line), std::unique_ptr<Expr>($2), @$.first_line);
            }
          | primary
            {
                $$ = $1;
            }
          ;

primary   : TOKEN_INT
            {
                $$ = new NumExpr($1, @$.first_line);
            }
          | TOKEN_ID
            {
                $$ = new VarExpr(*$1, @$.first_line);
                delete $1;
            }
          | TOKEN_ID '(' args ')'
            {
                $$ = new CallExpr(*$1, std::move(*$3), @$.first_line);
                delete $1;
                delete $3;
            }
          | TOKEN_BOX '[' args ']'
            {
                $$ = new ArrayLitExpr(std::move(*$3), @$.first_line);
                delete $3;
            }
          | TOKEN_INDEX '(' expr ',' expr ')'
            {
                $$ = new IndexExpr(std::unique_ptr<Expr>($3), std::unique_ptr<Expr>($5), @$.first_line);
            }
          | TOKEN_LEN '(' expr ')'
            {
                $$ = new LenExpr(std::unique_ptr<Expr>($3), @$.first_line);
            }
          | TOKEN_SLICE '(' expr ',' expr ',' expr ')'
            {
                $$ = new SliceExpr(std::unique_ptr<Expr>($3), std::unique_ptr<Expr>($5), std::unique_ptr<Expr>($7), @$.first_line);
            }
          | '(' expr ')'
            {
                $$ = $2;
            }
          ;

args      : /* empty */
            {
                $$ = new std::vector<std::unique_ptr<Expr>>();
            }
          | arg_list
            {
                $$ = $1;
            }
          ;

arg_list  : expr
            {
                $$ = new std::vector<std::unique_ptr<Expr>>();
                $$->push_back(std::unique_ptr<Expr>($1));
            }
          | arg_list ',' expr
            {
                $1->push_back(std::unique_ptr<Expr>($3));
                $$ = $1;
            }
          ;

%%

void yyerror(const char* s) {
    std::cerr << "Syntax error at line " << yylloc.first_line << ": " << s << "\n";
}
