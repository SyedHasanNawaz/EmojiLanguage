#pragma once
#include "ast.h"
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <stdexcept>

using Value = std::variant<long long, std::vector<long long>>;

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

    Value get_var(const std::string& name, int line);
    void set_var(const std::string& name, const Value& val, bool has_inbox, int line);
    void declare_var(const std::string& name, const Value& val, int line);

    static bool is_truthy(const Value& v, int line);
    static void print_value(std::ostream& os, const Value& val);

private:
    std::vector<std::unordered_map<std::string, Value>> env;
    std::unordered_map<std::string, const FuncDefStmt*> functions;
};
