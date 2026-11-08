#include "interp.h"

namespace {
struct ScopeGuard {
    Interpreter& interp;
    ScopeGuard(Interpreter& i) : interp(i) { interp.push_scope(); }
    ~ScopeGuard() { interp.pop_scope(); }
};
}

Interpreter::Interpreter() {
    env.push_back({}); // global scope
}

void Interpreter::push_scope() {
    env.push_back({});
}

void Interpreter::pop_scope() {
    if (env.size() > 1) {
        env.pop_back();
    }
}

bool Interpreter::is_truthy(const Value& v, int line) {
    if (std::holds_alternative<long long>(v)) {
        return std::get<long long>(v) != 0;
    }
    throw std::runtime_error("Line " + std::to_string(line) + ": Condition must evaluate to an integer");
}

void Interpreter::print_value(std::ostream& os, const Value& val) {
    if (std::holds_alternative<long long>(val)) {
        os << std::get<long long>(val);
    } else {
        const auto& arr = std::get<std::vector<long long>>(val);
        os << "[";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) os << ", ";
            os << arr[i];
        }
        os << "]";
    }
}

Value Interpreter::get_var(const std::string& name, int line) {
    for (auto it = env.rbegin(); it != env.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    throw std::runtime_error("Line " + std::to_string(line) + ": Undefined variable: " + name);
}

void Interpreter::set_var(const std::string& name, const Value& val, bool has_inbox, int line) {
    for (auto it = env.rbegin(); it != env.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            if (has_inbox && std::holds_alternative<std::vector<long long>>(found->second)) {
                auto& arr = std::get<std::vector<long long>>(found->second);
                if (std::holds_alternative<std::vector<long long>>(val)) {
                    const auto& src = std::get<std::vector<long long>>(val);
                    arr.insert(arr.end(), src.begin(), src.end());
                } else {
                    arr.push_back(std::get<long long>(val));
                }
            } else {
                found->second = val;
            }
            return;
        }
    }
    throw std::runtime_error("Line " + std::to_string(line) + ": Undefined variable: " + name);
}

void Interpreter::declare_var(const std::string& name, const Value& val, int /*line*/) {
    env.back()[name] = val;
}

void Interpreter::execute_program(const Program& program) {
    // Register top-level functions first
    for (const auto& stmt : program.stmts) {
        if (auto* fn = dynamic_cast<const FuncDefStmt*>(stmt.get())) {
            functions[fn->name] = fn;
        }
    }

    try {
        for (const auto& stmt : program.stmts) {
            execute(*stmt);
        }
    } catch (const ReturnSignal&) {
        // Top-level return exits program cleanly
    }
}

void Interpreter::execute(const Stmt& stmt) {
    if (auto* decl = dynamic_cast<const DeclStmt*>(&stmt)) {
        Value val = eval(*decl->init);
        declare_var(decl->name, val, decl->line);
    } else if (auto* assign = dynamic_cast<const AssignStmt*>(&stmt)) {
        Value val = eval(*assign->val);
        set_var(assign->name, val, assign->has_inbox, assign->line);
    } else if (auto* ifstmt = dynamic_cast<const IfStmt*>(&stmt)) {
        Value cond = eval(*ifstmt->cond);
        if (is_truthy(cond, ifstmt->line)) {
            ScopeGuard guard(*this);
            for (const auto& s : ifstmt->then_block->stmts) {
                execute(*s);
            }
        } else if (ifstmt->else_block) {
            ScopeGuard guard(*this);
            for (const auto& s : ifstmt->else_block->stmts) {
                execute(*s);
            }
        }
    } else if (auto* whilestmt = dynamic_cast<const WhileStmt*>(&stmt)) {
        while (is_truthy(eval(*whilestmt->cond), whilestmt->line)) {
            ScopeGuard guard(*this);
            for (const auto& s : whilestmt->body->stmts) {
                execute(*s);
            }
        }
    } else if (auto* funcdef = dynamic_cast<const FuncDefStmt*>(&stmt)) {
        functions[funcdef->name] = funcdef;
    } else if (auto* retstmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
        Value val = eval(*retstmt->val);
        throw ReturnSignal{val};
    } else if (auto* printstmt = dynamic_cast<const PrintStmt*>(&stmt)) {
        Value val = eval(*printstmt->val);
        print_value(std::cout, val);
        std::cout << "\n";
    } else if (auto* exprstmt = dynamic_cast<const ExprStmt*>(&stmt)) {
        eval(*exprstmt->expr);
    } else if (auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
        ScopeGuard guard(*this);
        for (const auto& s : block->stmts) {
            execute(*s);
        }
    }
}

Value Interpreter::eval(const Expr& expr) {
    if (auto* num = dynamic_cast<const NumExpr*>(&expr)) {
        return num->val;
    } else if (auto* var = dynamic_cast<const VarExpr*>(&expr)) {
        return get_var(var->name, var->line);
    } else if (auto* lit = dynamic_cast<const ArrayLitExpr*>(&expr)) {
        std::vector<long long> elems;
        for (const auto& e : lit->elements) {
            Value v = eval(*e);
            if (!std::holds_alternative<long long>(v)) {
                throw std::runtime_error("Line " + std::to_string(lit->line) + ": Array elements must be integers");
            }
            elems.push_back(std::get<long long>(v));
        }
        return elems;
    } else if (auto* idx_expr = dynamic_cast<const IndexExpr*>(&expr)) {
        Value av = eval(*idx_expr->arr);
        Value iv = eval(*idx_expr->index);
        if (!std::holds_alternative<std::vector<long long>>(av)) {
            throw std::runtime_error("Line " + std::to_string(idx_expr->line) + ": Index target must be an array");
        }
        if (!std::holds_alternative<long long>(iv)) {
            throw std::runtime_error("Line " + std::to_string(idx_expr->line) + ": Array index must be an integer");
        }
        const auto& arr = std::get<std::vector<long long>>(av);
        long long idx = std::get<long long>(iv);
        if (idx < 0 || idx >= static_cast<long long>(arr.size())) {
            throw std::runtime_error("Line " + std::to_string(idx_expr->line) + ": Array index out of bounds: " + std::to_string(idx));
        }
        return arr[idx];
    } else if (auto* len_expr = dynamic_cast<const LenExpr*>(&expr)) {
        Value av = eval(*len_expr->arr);
        if (!std::holds_alternative<std::vector<long long>>(av)) {
            throw std::runtime_error("Line " + std::to_string(len_expr->line) + ": Length target must be an array");
        }
        return static_cast<long long>(std::get<std::vector<long long>>(av).size());
    } else if (auto* sl_expr = dynamic_cast<const SliceExpr*>(&expr)) {
        Value av = eval(*sl_expr->arr);
        Value sv = eval(*sl_expr->start);
        Value ev = eval(*sl_expr->end);
        if (!std::holds_alternative<std::vector<long long>>(av)) {
            throw std::runtime_error("Line " + std::to_string(sl_expr->line) + ": Slice target must be an array");
        }
        if (!std::holds_alternative<long long>(sv) || !std::holds_alternative<long long>(ev)) {
            throw std::runtime_error("Line " + std::to_string(sl_expr->line) + ": Slice indices must be integers");
        }
        const auto& arr = std::get<std::vector<long long>>(av);
        long long s = std::get<long long>(sv);
        long long e = std::get<long long>(ev);
        long long len = static_cast<long long>(arr.size());
        if (s < 0) s = 0;
        if (s > len) s = len;
        if (e < s) e = s;
        if (e > len) e = len;
        return std::vector<long long>(arr.begin() + s, arr.begin() + e);
    } else if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = functions.find(call->callee);
        if (it == functions.end()) {
            throw std::runtime_error("Line " + std::to_string(call->line) + ": Undefined function: " + call->callee);
        }
        const FuncDefStmt* fn = it->second;
        if (call->args.size() != fn->params.size()) {
            throw std::runtime_error("Line " + std::to_string(call->line) + ": Function " + call->callee + " expects " +
                                     std::to_string(fn->params.size()) + " arguments, got " + std::to_string(call->args.size()));
        }
        std::vector<Value> arg_vals;
        for (const auto& a : call->args) {
            arg_vals.push_back(eval(*a));
        }

        auto saved_env = std::move(env);
        env.push_back(saved_env[0]); // preserve globals
        push_scope(); // function parameter and local scope
        for (size_t i = 0; i < fn->params.size(); ++i) {
            env.back()[fn->params[i]] = arg_vals[i];
        }

        Value ret = 0LL;
        try {
            for (const auto& s : fn->body->stmts) {
                execute(*s);
            }
        } catch (const ReturnSignal& sig) {
            ret = sig.v;
        }

        saved_env[0] = std::move(env[0]); // reflect global modifications
        env = std::move(saved_env);
        return ret;
    } else if (auto* bin = dynamic_cast<const BinOpExpr*>(&expr)) {
        // Short-circuit logical operators
        if (bin->op == "&&") {
            Value l = eval(*bin->left);
            if (!is_truthy(l, bin->line)) return 0LL;
            Value r = eval(*bin->right);
            return is_truthy(r, bin->line) ? 1LL : 0LL;
        }
        if (bin->op == "||") {
            Value l = eval(*bin->left);
            if (is_truthy(l, bin->line)) return 1LL;
            Value r = eval(*bin->right);
            return is_truthy(r, bin->line) ? 1LL : 0LL;
        }

        Value lv = eval(*bin->left);
        Value rv = eval(*bin->right);

        if (bin->op == "+") {
            if (std::holds_alternative<long long>(lv) && std::holds_alternative<long long>(rv)) {
                return std::get<long long>(lv) + std::get<long long>(rv);
            }
            if (std::holds_alternative<std::vector<long long>>(lv) && std::holds_alternative<std::vector<long long>>(rv)) {
                auto res = std::get<std::vector<long long>>(lv);
                const auto& r_arr = std::get<std::vector<long long>>(rv);
                res.insert(res.end(), r_arr.begin(), r_arr.end());
                return res;
            }
            if (std::holds_alternative<std::vector<long long>>(lv) && std::holds_alternative<long long>(rv)) {
                auto res = std::get<std::vector<long long>>(lv);
                res.push_back(std::get<long long>(rv));
                return res;
            }
            if (std::holds_alternative<long long>(lv) && std::holds_alternative<std::vector<long long>>(rv)) {
                auto res = std::get<std::vector<long long>>(rv);
                res.insert(res.begin(), std::get<long long>(lv));
                return res;
            }
        }

        if (bin->op == "==") {
            return (lv == rv) ? 1LL : 0LL;
        }
        if (bin->op == "!=") {
            return (lv != rv) ? 1LL : 0LL;
        }

        if (!std::holds_alternative<long long>(lv) || !std::holds_alternative<long long>(rv)) {
            throw std::runtime_error("Line " + std::to_string(bin->line) + ": Operator '" + bin->op + "' requires integer operands");
        }

        long long l = std::get<long long>(lv);
        long long r = std::get<long long>(rv);

        if (bin->op == "-") return l - r;
        if (bin->op == "*") return l * r;
        if (bin->op == "/") {
            if (r == 0) throw std::runtime_error("Line " + std::to_string(bin->line) + ": Division by zero");
            return l / r;
        }
        if (bin->op == "%") {
            if (r == 0) throw std::runtime_error("Line " + std::to_string(bin->line) + ": Modulo by zero");
            return l % r;
        }
        if (bin->op == "<") return l < r ? 1LL : 0LL;
        if (bin->op == ">") return l > r ? 1LL : 0LL;
        if (bin->op == "<=") return l <= r ? 1LL : 0LL;
        if (bin->op == ">=") return l >= r ? 1LL : 0LL;

        throw std::runtime_error("Line " + std::to_string(bin->line) + ": Unknown operator: " + bin->op);
    }

    throw std::runtime_error("Line " + std::to_string(expr.line) + ": Unknown expression type");
}
