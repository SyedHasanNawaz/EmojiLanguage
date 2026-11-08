#pragma once
#include <string>
#include <vector>
#include <memory>

struct Node {
    int line = 0;
    virtual ~Node() = default;
};

struct Expr : public Node {};

struct NumExpr : public Expr {
    long long val;
    NumExpr(long long v, int l = 0) : val(v) { line = l; }
};

struct VarExpr : public Expr {
    std::string name;
    VarExpr(std::string n, int l = 0) : name(std::move(n)) { line = l; }
};

struct BinOpExpr : public Expr {
    std::string op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    BinOpExpr(std::string o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r, int l_num = 0)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) { line = l_num; }
};

struct CallExpr : public Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::string c, std::vector<std::unique_ptr<Expr>> a, int l_num = 0)
        : callee(std::move(c)), args(std::move(a)) { line = l_num; }
};

struct ArrayLitExpr : public Expr {
    std::vector<std::unique_ptr<Expr>> elements;
    ArrayLitExpr(std::vector<std::unique_ptr<Expr>> el, int l_num = 0)
        : elements(std::move(el)) { line = l_num; }
};

struct IndexExpr : public Expr {
    std::unique_ptr<Expr> arr;
    std::unique_ptr<Expr> index;
    IndexExpr(std::unique_ptr<Expr> a, std::unique_ptr<Expr> idx, int l_num = 0)
        : arr(std::move(a)), index(std::move(idx)) { line = l_num; }
};

struct LenExpr : public Expr {
    std::unique_ptr<Expr> arr;
    LenExpr(std::unique_ptr<Expr> a, int l_num = 0)
        : arr(std::move(a)) { line = l_num; }
};

struct SliceExpr : public Expr {
    std::unique_ptr<Expr> arr;
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
    SliceExpr(std::unique_ptr<Expr> a, std::unique_ptr<Expr> s, std::unique_ptr<Expr> e, int l_num = 0)
        : arr(std::move(a)), start(std::move(s)), end(std::move(e)) { line = l_num; }
};

struct Stmt : public Node {};

struct BlockStmt : public Stmt {
    std::vector<std::unique_ptr<Stmt>> stmts;
    BlockStmt(std::vector<std::unique_ptr<Stmt>> s, int l_num = 0)
        : stmts(std::move(s)) { line = l_num; }
};

struct DeclStmt : public Stmt {
    std::string name;
    std::unique_ptr<Expr> init;
    DeclStmt(std::string n, std::unique_ptr<Expr> i, int l_num = 0)
        : name(std::move(n)), init(std::move(i)) { line = l_num; }
};

struct AssignStmt : public Stmt {
    std::string name;
    std::unique_ptr<Expr> val;
    bool has_inbox;
    AssignStmt(std::string n, std::unique_ptr<Expr> v, bool inbox, int l_num = 0)
        : name(std::move(n)), val(std::move(v)), has_inbox(inbox) { line = l_num; }
};

struct IfStmt : public Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<BlockStmt> then_block;
    std::unique_ptr<BlockStmt> else_block;
    IfStmt(std::unique_ptr<Expr> c, std::unique_ptr<BlockStmt> t, std::unique_ptr<BlockStmt> e, int l_num = 0)
        : cond(std::move(c)), then_block(std::move(t)), else_block(std::move(e)) { line = l_num; }
};

struct WhileStmt : public Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<BlockStmt> body;
    WhileStmt(std::unique_ptr<Expr> c, std::unique_ptr<BlockStmt> b, int l_num = 0)
        : cond(std::move(c)), body(std::move(b)) { line = l_num; }
};

struct FuncDefStmt : public Stmt {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockStmt> body;
    FuncDefStmt(std::string n, std::vector<std::string> p, std::unique_ptr<BlockStmt> b, int l_num = 0)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)) { line = l_num; }
};

struct ReturnStmt : public Stmt {
    std::unique_ptr<Expr> val;
    ReturnStmt(std::unique_ptr<Expr> v, int l_num = 0)
        : val(std::move(v)) { line = l_num; }
};

struct PrintStmt : public Stmt {
    std::unique_ptr<Expr> val;
    PrintStmt(std::unique_ptr<Expr> v, int l_num = 0)
        : val(std::move(v)) { line = l_num; }
};

struct ExprStmt : public Stmt {
    std::unique_ptr<Expr> expr;
    ExprStmt(std::unique_ptr<Expr> e, int l_num = 0)
        : expr(std::move(e)) { line = l_num; }
};

struct Program : public Node {
    std::vector<std::unique_ptr<Stmt>> stmts;
    Program(std::vector<std::unique_ptr<Stmt>> s, int l_num = 0)
        : stmts(std::move(s)) { line = l_num; }
};
