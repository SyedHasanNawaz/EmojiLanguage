#pragma once

#include "ast.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

using namespace std;

enum class TypeKind {
    Unknown,
    Int,
    Array
};

struct VarInfo {
    llvm::AllocaInst* alloca_inst = nullptr;
    TypeKind type_kind = TypeKind::Int;
};

struct FuncInfo {
    string name;
    vector<TypeKind> param_types;
    TypeKind ret_type = TypeKind::Unknown;
    const FuncDefStmt* ast_node = nullptr;
    llvm::Function* llvm_fn = nullptr;
};

class CodeGen {
public:
    CodeGen();
    ~CodeGen() = default;

    void generate(const Program& program);
    void dump_ir(ostream& os);
    void print_ir();
    bool emit_object(const string& obj_path);
    bool emit_native(const string& exe_path);

private:
    llvm::LLVMContext ctx;
    unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;

    llvm::Type* i64_type = nullptr;
    llvm::Type* i32_type = nullptr;
    llvm::Type* void_type = nullptr;
    llvm::PointerType* ptr_type = nullptr;
    llvm::StructType* array_type = nullptr;

    llvm::Function* printf_fn = nullptr;
    llvm::Function* calloc_fn = nullptr;
    llvm::Function* free_fn = nullptr;
    llvm::Function* exit_fn = nullptr;

    llvm::Function* print_int_fn = nullptr;
    llvm::Function* print_array_fn = nullptr;
    llvm::Function* slice_fn = nullptr;
    llvm::Function* array_concat_fn = nullptr;
    llvm::Function* array_push_fn = nullptr;
    llvm::Function* array_prepend_fn = nullptr;
    llvm::Function* bounds_check_fn = nullptr;
    llvm::Function* array_eq_fn = nullptr;
    llvm::Function* array_ne_fn = nullptr;

    llvm::Value* fmt_int_ptr = nullptr;
    llvm::Value* fmt_open_ptr = nullptr;
    llvm::Value* fmt_close_ptr = nullptr;
    llvm::Value* fmt_sep_ptr = nullptr;
    llvm::Value* fmt_elem_ptr = nullptr;
    llvm::Value* fmt_bounds_err_ptr = nullptr;

    vector<unordered_map<string, VarInfo>> scopes;
    unordered_map<string, FuncInfo> funcs;
    llvm::Function* current_function = nullptr;

    void emit_runtime_helpers();
    void infer_types(const Program& program);
    TypeKind infer_expr_type(const Expr& expr, const unordered_map<string, TypeKind>& local_types);

    void push_scope();
    void pop_scope();
    VarInfo lookup_var(const string& name, int line);
    llvm::AllocaInst* create_entry_block_alloca(llvm::Function* fn, const string& name, llvm::Type* type);

    void codegen_func_def(const FuncDefStmt& func_def);
    void codegen_stmt(const Stmt& stmt);
    llvm::Value* codegen_expr(const Expr& expr);
    TypeKind get_expr_type(const Expr& expr);
};
