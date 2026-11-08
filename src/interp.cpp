#include "interp.h"

using namespace std;

namespace {
struct ScopeGuard {
    Interpreter& interp;
    ScopeGuard(Interpreter& i) : interp(i) { 
        interp.push_scope(); 
    }
    ~ScopeGuard() { 
        interp.pop_scope(); 
    }
};
}

Interpreter::Interpreter() {
    env.push_back({});
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
    if (holds_alternative<long long>(v)) {
        return get<long long>(v) != 0;
    }
    throw runtime_error("Line " + to_string(line) + ": Condition must evaluate to an integer");
}

void Interpreter::print_value(ostream& os, const Value& val) {
    if (holds_alternative<long long>(val)) {
        os << get<long long>(val);
    } else {
        const auto& arr = get<vector<long long>>(val);
        os << "[";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) os << ", ";
            os << arr[i];
        }
        os << "]";
    }
}

Value Interpreter::get_var(const string& name, int line) {
    for (auto it = env.rbegin(); it != env.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) {
            return f->second;
        }
    }
    throw runtime_error("Line " + to_string(line) + ": Undefined variable: " + name);
}

void Interpreter::set_var(const string& name, const Value& val, bool has_inbox, int line) {
    for (auto it = env.rbegin(); it != env.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) {
            if (has_inbox && holds_alternative<vector<long long>>(f->second)) {
                auto& arr = get<vector<long long>>(f->second);
                if (holds_alternative<vector<long long>>(val)) {
                    const auto& src = get<vector<long long>>(val);
                    arr.insert(arr.end(), src.begin(), src.end());
                } else {
                    arr.push_back(get<long long>(val));
                }
            } else {
                f->second = val;
            }
            return;
        }
    }
    throw runtime_error("Line " + to_string(line) + ": Undefined variable: " + name);
}

void Interpreter::declare_var(const string& name, const Value& val, int) {
    env.back()[name] = val;
}

void Interpreter::execute_program(const Program& program) {
    for (const auto& s : program.stmts) {
        if (auto* fn = dynamic_cast<const FuncDefStmt*>(s.get())) {
            functions[fn->name] = fn;
        }
    }

    try {
        for (const auto& s : program.stmts) {
            execute(*s);
        }
    } catch (const ReturnSignal&) {
    }
}

void Interpreter::execute(const Stmt& stmt) {
    if (auto* d = dynamic_cast<const DeclStmt*>(&stmt)) {
        Value val = eval(*d->init);
        declare_var(d->name, val, d->line);
    } 
    else if (auto* a = dynamic_cast<const AssignStmt*>(&stmt)) {
        Value val = eval(*a->val);
        set_var(a->name, val, a->has_inbox, a->line);
    } 
    else if (auto* ifs = dynamic_cast<const IfStmt*>(&stmt)) {
        Value c = eval(*ifs->cond);
        if (is_truthy(c, ifs->line)) {
            ScopeGuard g(*this);
            for (const auto& s : ifs->then_block->stmts) {
                execute(*s);
            }
        } else if (ifs->else_block) {
            ScopeGuard g(*this);
            for (const auto& s : ifs->else_block->stmts) {
                execute(*s);
            }
        }
    } 
    else if (auto* ws = dynamic_cast<const WhileStmt*>(&stmt)) {
        while (is_truthy(eval(*ws->cond), ws->line)) {
            ScopeGuard g(*this);
            for (const auto& s : ws->body->stmts) {
                execute(*s);
            }
        }
    } 
    else if (auto* f = dynamic_cast<const FuncDefStmt*>(&stmt)) {
        functions[f->name] = f;
    } 
    else if (auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) {
        Value val = eval(*r->val);
        throw ReturnSignal{val};
    } 
    else if (auto* p = dynamic_cast<const PrintStmt*>(&stmt)) {
        Value val = eval(*p->val);
        print_value(cout, val);
        cout << "\n";
    } 
    else if (auto* e = dynamic_cast<const ExprStmt*>(&stmt)) {
        eval(*e->expr);
    } 
    else if (auto* b = dynamic_cast<const BlockStmt*>(&stmt)) {
        ScopeGuard g(*this);
        for (const auto& s : b->stmts) {
            execute(*s);
        }
    }
}

Value Interpreter::eval(const Expr& expr) {
    if (auto* num = dynamic_cast<const NumExpr*>(&expr)) {
        return num->val;
    } 
    else if (auto* var = dynamic_cast<const VarExpr*>(&expr)) {
        return get_var(var->name, var->line);
    } 
    else if (auto* lit = dynamic_cast<const ArrayLitExpr*>(&expr)) {
        vector<long long> elems;
        for (const auto& e : lit->elements) {
            Value v = eval(*e);
            if (!holds_alternative<long long>(v)) {
                throw runtime_error("Line " + to_string(lit->line) + ": Array elements must be integers");
            }
            elems.push_back(get<long long>(v));
        }
        return elems;
    } 
    else if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        Value av = eval(*idx->arr);
        Value iv = eval(*idx->index);
        if (!holds_alternative<vector<long long>>(av)) {
            throw runtime_error("Line " + to_string(idx->line) + ": Index target must be an array");
        }
        if (!holds_alternative<long long>(iv)) {
            throw runtime_error("Line " + to_string(idx->line) + ": Array index must be an integer");
        }
        const auto& arr = get<vector<long long>>(av);
        long long pos = get<long long>(iv);
        if (pos < 0 || pos >= static_cast<long long>(arr.size())) {
            throw runtime_error("Line " + to_string(idx->line) + ": Array index out of bounds: " + to_string(pos));
        }
        return arr[pos];
    } 
    else if (auto* len = dynamic_cast<const LenExpr*>(&expr)) {
        Value av = eval(*len->arr);
        if (!holds_alternative<vector<long long>>(av)) {
            throw runtime_error("Line " + to_string(len->line) + ": Length target must be an array");
        }
        return static_cast<long long>(get<vector<long long>>(av).size());
    } 
    else if (auto* sl = dynamic_cast<const SliceExpr*>(&expr)) {
        Value av = eval(*sl->arr);
        Value sv = eval(*sl->start);
        Value ev = eval(*sl->end);
        if (!holds_alternative<vector<long long>>(av)) {
            throw runtime_error("Line " + to_string(sl->line) + ": Slice target must be an array");
        }
        if (!holds_alternative<long long>(sv) || !holds_alternative<long long>(ev)) {
            throw runtime_error("Line " + to_string(sl->line) + ": Slice indices must be integers");
        }
        const auto& arr = get<vector<long long>>(av);
        long long s = get<long long>(sv);
        long long e = get<long long>(ev);
        long long total = static_cast<long long>(arr.size());
        if (s < 0) s = 0;
        if (s > total) s = total;
        if (e < s) e = s;
        if (e > total) e = total;
        return vector<long long>(arr.begin() + s, arr.begin() + e);
    } 
    else if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = functions.find(call->callee);
        if (it == functions.end()) {
            throw runtime_error("Line " + to_string(call->line) + ": Undefined function: " + call->callee);
        }
        const FuncDefStmt* fn = it->second;
        if (call->args.size() != fn->params.size()) {
            throw runtime_error("Line " + to_string(call->line) + ": Function " + call->callee + " expects " +
                               to_string(fn->params.size()) + " arguments, got " + to_string(call->args.size()));
        }
        vector<Value> arg_vals;
        for (const auto& a : call->args) {
            arg_vals.push_back(eval(*a));
        }

        auto saved = move(env);
        env.push_back(saved[0]);
        push_scope();
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

        saved[0] = move(env[0]);
        env = move(saved);
        return ret;
    } 
    else if (auto* b = dynamic_cast<const BinOpExpr*>(&expr)) {
        if (b->op == "&&") {
            Value l = eval(*b->left);
            if (!is_truthy(l, b->line)) return 0LL;
            Value r = eval(*b->right);
            return is_truthy(r, b->line) ? 1LL : 0LL;
        }
        if (b->op == "||") {
            Value l = eval(*b->left);
            if (is_truthy(l, b->line)) return 1LL;
            Value r = eval(*b->right);
            return is_truthy(r, b->line) ? 1LL : 0LL;
        }

        Value lv = eval(*b->left);
        Value rv = eval(*b->right);

        if (b->op == "+") {
            if (holds_alternative<long long>(lv) && holds_alternative<long long>(rv)) {
                return get<long long>(lv) + get<long long>(rv);
            }
            if (holds_alternative<vector<long long>>(lv) && holds_alternative<vector<long long>>(rv)) {
                auto res = get<vector<long long>>(lv);
                const auto& r_arr = get<vector<long long>>(rv);
                res.insert(res.end(), r_arr.begin(), r_arr.end());
                return res;
            }
            if (holds_alternative<vector<long long>>(lv) && holds_alternative<long long>(rv)) {
                auto res = get<vector<long long>>(lv);
                res.push_back(get<long long>(rv));
                return res;
            }
            if (holds_alternative<long long>(lv) && holds_alternative<vector<long long>>(rv)) {
                auto res = get<vector<long long>>(rv);
                res.insert(res.begin(), get<long long>(lv));
                return res;
            }
        }

        if (b->op == "==") {
            return (lv == rv) ? 1LL : 0LL;
        }
        if (b->op == "!=") {
            return (lv != rv) ? 1LL : 0LL;
        }

        if (!holds_alternative<long long>(lv) || !holds_alternative<long long>(rv)) {
            throw runtime_error("Line " + to_string(b->line) + ": Operator '" + b->op + "' requires integer operands");
        }

        long long l = get<long long>(lv);
        long long r = get<long long>(rv);

        if (b->op == "-") return l - r;
        if (b->op == "*") return l * r;
        if (b->op == "/") {
            if (r == 0) throw runtime_error("Line " + to_string(b->line) + ": Division by zero");
            return l / r;
        }
        if (b->op == "%") {
            if (r == 0) throw runtime_error("Line " + to_string(b->line) + ": Modulo by zero");
            return l % r;
        }
        if (b->op == "<") return l < r ? 1LL : 0LL;
        if (b->op == ">") return l > r ? 1LL : 0LL;
        if (b->op == "<=") return l <= r ? 1LL : 0LL;
        if (b->op == ">=") return l >= r ? 1LL : 0LL;

        throw runtime_error("Line " + to_string(b->line) + ": Unknown operator: " + b->op);
    }

    throw runtime_error("Line " + to_string(expr.line) + ": Unknown expression type");
}
