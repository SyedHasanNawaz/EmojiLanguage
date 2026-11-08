#pragma once

#include "ast.h"
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <stdexcept>

using namespace std;

using Value = variant<long long, vector<long long>>;

struct ReturnSignal {
    Value v;
};

class Interpreter {
public:
    Interpreter();

    void execute_program(const Program& program);
    Value eval(const Expr& expr);
    void execute(const Stmt& stmt);

    void push_scope();
    void pop_scope();

    Value get_var(const string& name, int line);
    void set_var(const string& name, const Value& val, bool has_inbox, int line);
    void declare_var(const string& name, const Value& val, int line);

    static bool is_truthy(const Value& v, int line);
    static void print_value(ostream& os, const Value& val);

private:
    vector<unordered_map<string, Value>> env;
    unordered_map<string, const FuncDefStmt*> functions;
};
