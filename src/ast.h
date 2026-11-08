#pragma once

#include <string>
#include <vector>
#include <memory>

using namespace std;

struct Node {
    int line = 0;
    virtual ~Node() = default;
};

struct Expr : public Node {
};

struct NumExpr : public Expr {
    long long val;

    NumExpr(long long v, int l = 0) : val(v) { 
        line = l; 
    }
};

struct VarExpr : public Expr {
    string name;

    VarExpr(string n, int l = 0) : name(move(n)) { 
        line = l; 
    }
};

struct BinOpExpr : public Expr {
    string op;
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;

    BinOpExpr(string o, unique_ptr<Expr> l, unique_ptr<Expr> r, int l_num = 0)
        : op(move(o)), left(move(l)), right(move(r)) {
        line = l_num;
    }
};

struct CallExpr : public Expr {
    string callee;
    vector<unique_ptr<Expr>> args;

    CallExpr(string c, vector<unique_ptr<Expr>> a, int l_num = 0)
        : callee(move(c)), args(move(a)) {
        line = l_num;
    }
};

struct ArrayLitExpr : public Expr {
    vector<unique_ptr<Expr>> elements;

    ArrayLitExpr(vector<unique_ptr<Expr>> el, int l_num = 0)
        : elements(move(el)) {
        line = l_num;
    }
};

struct IndexExpr : public Expr {
    unique_ptr<Expr> arr;
    unique_ptr<Expr> index;

    IndexExpr(unique_ptr<Expr> a, unique_ptr<Expr> idx, int l_num = 0)
        : arr(move(a)), index(move(idx)) {
        line = l_num;
    }
};

struct LenExpr : public Expr {
    unique_ptr<Expr> arr;

    LenExpr(unique_ptr<Expr> a, int l_num = 0)
        : arr(move(a)) {
        line = l_num;
    }
};

struct SliceExpr : public Expr {
    unique_ptr<Expr> arr;
    unique_ptr<Expr> start;
    unique_ptr<Expr> end;

    SliceExpr(unique_ptr<Expr> a, unique_ptr<Expr> s, unique_ptr<Expr> e, int l_num = 0)
        : arr(move(a)), start(move(s)), end(move(e)) {
        line = l_num;
    }
};



struct Stmt : public Node {
};

struct BlockStmt : public Stmt {
    vector<unique_ptr<Stmt>> stmts;

    BlockStmt(vector<unique_ptr<Stmt>> s, int l_num = 0)
        : stmts(move(s)) {
        line = l_num;
    }
};

struct DeclStmt : public Stmt {
    string name;
    unique_ptr<Expr> init;

    DeclStmt(string n, unique_ptr<Expr> i, int l_num = 0)
        : name(move(n)), init(move(i)) {
        line = l_num;
    }
};

struct AssignStmt : public Stmt {
    string name;
    unique_ptr<Expr> val;
    bool has_inbox;

    AssignStmt(string n, unique_ptr<Expr> v, bool inbox, int l_num = 0)
        : name(move(n)), val(move(v)), has_inbox(inbox) {
        line = l_num;
    }
};

struct IfStmt : public Stmt {
    unique_ptr<Expr> cond;
    unique_ptr<BlockStmt> then_block;
    unique_ptr<BlockStmt> else_block;

    IfStmt(unique_ptr<Expr> c, unique_ptr<BlockStmt> t, unique_ptr<BlockStmt> e, int l_num = 0)
        : cond(move(c)), then_block(move(t)), else_block(move(e)) {
        line = l_num;
    }
};

struct WhileStmt : public Stmt {
    unique_ptr<Expr> cond;
    unique_ptr<BlockStmt> body;

    WhileStmt(unique_ptr<Expr> c, unique_ptr<BlockStmt> b, int l_num = 0)
        : cond(move(c)), body(move(b)) {
        line = l_num;
    }
};

struct FuncDefStmt : public Stmt {
    string name;
    vector<string> params;
    unique_ptr<BlockStmt> body;

    FuncDefStmt(string n, vector<string> p, unique_ptr<BlockStmt> b, int l_num = 0)
        : name(move(n)), params(move(p)), body(move(b)) {
        line = l_num;
    }
};

struct ReturnStmt : public Stmt {
    unique_ptr<Expr> val;

    ReturnStmt(unique_ptr<Expr> v, int l_num = 0)
        : val(move(v)) {
        line = l_num;
    }
};

struct PrintStmt : public Stmt {
    unique_ptr<Expr> val;

    PrintStmt(unique_ptr<Expr> v, int l_num = 0)
        : val(move(v)) {
        line = l_num;
    }
};

struct ExprStmt : public Stmt {
    unique_ptr<Expr> expr;

    ExprStmt(unique_ptr<Expr> e, int l_num = 0)
        : expr(move(e)) {
        line = l_num;
    }
};



struct Program : public Node {
    vector<unique_ptr<Stmt>> stmts;

    Program(vector<unique_ptr<Stmt>> s, int l_num = 0)
        : stmts(move(s)) {
        line = l_num;
    }
};
