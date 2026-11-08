#include "codegen.h"
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdlib>
#include <unistd.h>
#include <stdexcept>

CodeGen::CodeGen() : builder(ctx) {
    module = std::make_unique<llvm::Module>("emojilang_module", ctx);

    i64_type = llvm::Type::getInt64Ty(ctx);
    i32_type = llvm::Type::getInt32Ty(ctx);
    void_type = llvm::Type::getVoidTy(ctx);
    ptr_type = llvm::PointerType::get(ctx, 0);
    array_type = llvm::StructType::get(ctx, { i64_type, ptr_type }, "Array");

    emit_runtime_helpers();
}

void CodeGen::emit_runtime_helpers() {
    printf_fn = llvm::Function::Create(
        llvm::FunctionType::get(i32_type, { ptr_type }, true),
        llvm::Function::ExternalLinkage, "printf", module.get());

    calloc_fn = llvm::Function::Create(
        llvm::FunctionType::get(ptr_type, { i64_type, i64_type }, false),
        llvm::Function::ExternalLinkage, "calloc", module.get());

    free_fn = llvm::Function::Create(
        llvm::FunctionType::get(void_type, { ptr_type }, false),
        llvm::Function::ExternalLinkage, "free", module.get());

    exit_fn = llvm::Function::Create(
        llvm::FunctionType::get(void_type, { i32_type }, false),
        llvm::Function::ExternalLinkage, "exit", module.get());

    fmt_int_ptr = builder.CreateGlobalString("%lld\n", ".fmt_int", 0, module.get());
    fmt_open_ptr = builder.CreateGlobalString("[", ".fmt_open", 0, module.get());
    fmt_close_ptr = builder.CreateGlobalString("]\n", ".fmt_close", 0, module.get());
    fmt_sep_ptr = builder.CreateGlobalString(", ", ".fmt_sep", 0, module.get());
    fmt_elem_ptr = builder.CreateGlobalString("%lld", ".fmt_elem", 0, module.get());
    fmt_bounds_err_ptr = builder.CreateGlobalString("Runtime Error: Array index out of bounds: %lld (length %lld)\n", ".fmt_bounds_err", 0, module.get());

    // emojilang_print_int(i64)
    {
        auto* fn_ty = llvm::FunctionType::get(void_type, { i64_type }, false);
        print_int_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_print_int", module.get());
        auto* bb = llvm::BasicBlock::Create(ctx, "entry", print_int_fn);
        llvm::IRBuilder<> b(bb);
        b.CreateCall(printf_fn, { fmt_int_ptr, print_int_fn->getArg(0) });
        b.CreateRetVoid();
    }

    // emojilang_print_array({i64, ptr})
    {
        auto* fn_ty = llvm::FunctionType::get(void_type, { array_type }, false);
        print_array_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_print_array", module.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", print_array_fn);
        auto* loop_cond_bb = llvm::BasicBlock::Create(ctx, "loop.cond", print_array_fn);
        auto* loop_body_bb = llvm::BasicBlock::Create(ctx, "loop.body", print_array_fn);
        auto* sep_bb = llvm::BasicBlock::Create(ctx, "loop.sep", print_array_fn);
        auto* elem_bb = llvm::BasicBlock::Create(ctx, "loop.elem", print_array_fn);
        auto* loop_inc_bb = llvm::BasicBlock::Create(ctx, "loop.inc", print_array_fn);
        auto* exit_bb = llvm::BasicBlock::Create(ctx, "exit", print_array_fn);

        llvm::IRBuilder<> b(entry_bb);
        auto* len = b.CreateExtractValue(print_array_fn->getArg(0), 0, "arr.len");
        auto* data = b.CreateExtractValue(print_array_fn->getArg(0), 1, "arr.data");
        b.CreateCall(printf_fn, { fmt_open_ptr });
        auto* i_alloc = b.CreateAlloca(i64_type, nullptr, "i");
        b.CreateStore(llvm::ConstantInt::get(i64_type, 0), i_alloc);
        b.CreateBr(loop_cond_bb);

        b.SetInsertPoint(loop_cond_bb);
        auto* cur_i = b.CreateLoad(i64_type, i_alloc, "cur.i");
        auto* has_more = b.CreateICmpSLT(cur_i, len, "has.more");
        b.CreateCondBr(has_more, loop_body_bb, exit_bb);

        b.SetInsertPoint(loop_body_bb);
        auto* is_not_first = b.CreateICmpSGT(cur_i, llvm::ConstantInt::get(i64_type, 0), "is.not.first");
        b.CreateCondBr(is_not_first, sep_bb, elem_bb);

        b.SetInsertPoint(sep_bb);
        b.CreateCall(printf_fn, { fmt_sep_ptr });
        b.CreateBr(elem_bb);

        b.SetInsertPoint(elem_bb);
        auto* gep = b.CreateGEP(i64_type, data, cur_i, "elem.gep");
        auto* elem = b.CreateLoad(i64_type, gep, "elem.val");
        b.CreateCall(printf_fn, { fmt_elem_ptr, elem });
        b.CreateBr(loop_inc_bb);

        b.SetInsertPoint(loop_inc_bb);
        auto* next_i = b.CreateAdd(cur_i, llvm::ConstantInt::get(i64_type, 1), "next.i");
        b.CreateStore(next_i, i_alloc);
        b.CreateBr(loop_cond_bb);

        b.SetInsertPoint(exit_bb);
        b.CreateCall(printf_fn, { fmt_close_ptr });
        b.CreateRetVoid();
    }

    // emojilang_bounds_check(i64 idx, i64 len)
    {
        auto* fn_ty = llvm::FunctionType::get(void_type, { i64_type, i64_type }, false);
        bounds_check_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_bounds_check", module.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", bounds_check_fn);
        auto* oob_bb = llvm::BasicBlock::Create(ctx, "oob", bounds_check_fn);
        auto* ok_bb = llvm::BasicBlock::Create(ctx, "ok", bounds_check_fn);

        llvm::IRBuilder<> b(entry_bb);
        auto* idx = bounds_check_fn->getArg(0);
        auto* len = bounds_check_fn->getArg(1);
        auto* c1 = b.CreateICmpSLT(idx, llvm::ConstantInt::get(i64_type, 0));
        auto* c2 = b.CreateICmpSGE(idx, len);
        auto* oob = b.CreateOr(c1, c2);
        b.CreateCondBr(oob, oob_bb, ok_bb);

        b.SetInsertPoint(oob_bb);
        b.CreateCall(printf_fn, { fmt_bounds_err_ptr, idx, len });
        b.CreateCall(exit_fn, { llvm::ConstantInt::get(i32_type, 1) });
        b.CreateUnreachable();

        b.SetInsertPoint(ok_bb);
        b.CreateRetVoid();
    }

    // emojilang_slice({i64, ptr}, i64, i64)
    {
        auto* fn_ty = llvm::FunctionType::get(array_type, { array_type, i64_type, i64_type }, false);
        slice_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_slice", module.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", slice_fn);
        auto* loop_cond_bb = llvm::BasicBlock::Create(ctx, "loop.cond", slice_fn);
        auto* loop_body_bb = llvm::BasicBlock::Create(ctx, "loop.body", slice_fn);
        auto* exit_bb = llvm::BasicBlock::Create(ctx, "exit", slice_fn);

        llvm::IRBuilder<> b(entry_bb);
        auto* len = b.CreateExtractValue(slice_fn->getArg(0), 0, "arr.len");
        auto* data = b.CreateExtractValue(slice_fn->getArg(0), 1, "arr.data");
        auto* raw_s = slice_fn->getArg(1);
        auto* raw_e = slice_fn->getArg(2);

        // s = max(0, min(raw_s, len))
        auto* s_lt_0 = b.CreateICmpSLT(raw_s, llvm::ConstantInt::get(i64_type, 0));
        auto* s1 = b.CreateSelect(s_lt_0, llvm::ConstantInt::get(i64_type, 0), raw_s);
        auto* s1_gt_len = b.CreateICmpSGT(s1, len);
        auto* s = b.CreateSelect(s1_gt_len, len, s1, "clamped.s");

        // e = max(s, min(raw_e, len))
        auto* e_lt_s = b.CreateICmpSLT(raw_e, s);
        auto* e1 = b.CreateSelect(e_lt_s, s, raw_e);
        auto* e1_gt_len = b.CreateICmpSGT(e1, len);
        auto* e = b.CreateSelect(e1_gt_len, len, e1, "clamped.e");

        auto* sz = b.CreateSub(e, s, "sz");
        auto* new_buf = b.CreateCall(calloc_fn, { sz, llvm::ConstantInt::get(i64_type, 8) }, "new.buf");

        auto* k_alloc = b.CreateAlloca(i64_type, nullptr, "k");
        b.CreateStore(llvm::ConstantInt::get(i64_type, 0), k_alloc);
        b.CreateBr(loop_cond_bb);

        b.SetInsertPoint(loop_cond_bb);
        auto* cur_k = b.CreateLoad(i64_type, k_alloc, "cur.k");
        auto* has_more = b.CreateICmpSLT(cur_k, sz);
        b.CreateCondBr(has_more, loop_body_bb, exit_bb);

        b.SetInsertPoint(loop_body_bb);
        auto* orig_idx = b.CreateAdd(s, cur_k, "orig.idx");
        auto* src_gep = b.CreateGEP(i64_type, data, orig_idx);
        auto* val = b.CreateLoad(i64_type, src_gep);
        auto* dst_gep = b.CreateGEP(i64_type, new_buf, cur_k);
        b.CreateStore(val, dst_gep);
        auto* next_k = b.CreateAdd(cur_k, llvm::ConstantInt::get(i64_type, 1));
        b.CreateStore(next_k, k_alloc);
        b.CreateBr(loop_cond_bb);

        b.SetInsertPoint(exit_bb);
        llvm::Value* ret_struct = llvm::UndefValue::get(array_type);
        ret_struct = b.CreateInsertValue(ret_struct, sz, 0);
        ret_struct = b.CreateInsertValue(ret_struct, new_buf, 1);
        b.CreateRet(ret_struct);
    }

    // emojilang_array_concat({i64, ptr}, {i64, ptr})
    {
        auto* fn_ty = llvm::FunctionType::get(array_type, { array_type, array_type }, false);
        array_concat_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_array_concat", module.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", array_concat_fn);
        auto* loop_a_cond = llvm::BasicBlock::Create(ctx, "loop.a.cond", array_concat_fn);
        auto* loop_a_body = llvm::BasicBlock::Create(ctx, "loop.a.body", array_concat_fn);
        auto* loop_b_cond = llvm::BasicBlock::Create(ctx, "loop.b.cond", array_concat_fn);
        auto* loop_b_body = llvm::BasicBlock::Create(ctx, "loop.b.body", array_concat_fn);
        auto* exit_bb = llvm::BasicBlock::Create(ctx, "exit", array_concat_fn);

        llvm::IRBuilder<> b(entry_bb);
        auto* len_a = b.CreateExtractValue(array_concat_fn->getArg(0), 0);
        auto* data_a = b.CreateExtractValue(array_concat_fn->getArg(0), 1);
        auto* len_b = b.CreateExtractValue(array_concat_fn->getArg(1), 0);
        auto* data_b = b.CreateExtractValue(array_concat_fn->getArg(1), 1);

        auto* new_len = b.CreateAdd(len_a, len_b);
        auto* new_buf = b.CreateCall(calloc_fn, { new_len, llvm::ConstantInt::get(i64_type, 8) });

        auto* i_alloc = b.CreateAlloca(i64_type, nullptr, "i");
        b.CreateStore(llvm::ConstantInt::get(i64_type, 0), i_alloc);
        b.CreateBr(loop_a_cond);

        b.SetInsertPoint(loop_a_cond);
        auto* cur_i = b.CreateLoad(i64_type, i_alloc);
        auto* has_more_a = b.CreateICmpSLT(cur_i, len_a);
        b.CreateCondBr(has_more_a, loop_a_body, loop_b_cond);

        b.SetInsertPoint(loop_a_body);
        auto* src_a_gep = b.CreateGEP(i64_type, data_a, cur_i);
        auto* val_a = b.CreateLoad(i64_type, src_a_gep);
        auto* dst_a_gep = b.CreateGEP(i64_type, new_buf, cur_i);
        b.CreateStore(val_a, dst_a_gep);
        b.CreateStore(b.CreateAdd(cur_i, llvm::ConstantInt::get(i64_type, 1)), i_alloc);
        b.CreateBr(loop_a_cond);

        b.SetInsertPoint(loop_b_cond);
        b.CreateStore(llvm::ConstantInt::get(i64_type, 0), i_alloc);
        auto* loop_b_real_cond = llvm::BasicBlock::Create(ctx, "loop.b.real.cond", array_concat_fn);
        b.CreateBr(loop_b_real_cond);

        b.SetInsertPoint(loop_b_real_cond);
        auto* cur_j = b.CreateLoad(i64_type, i_alloc);
        auto* has_more_b = b.CreateICmpSLT(cur_j, len_b);
        b.CreateCondBr(has_more_b, loop_b_body, exit_bb);

        b.SetInsertPoint(loop_b_body);
        auto* src_b_gep = b.CreateGEP(i64_type, data_b, cur_j);
        auto* val_b = b.CreateLoad(i64_type, src_b_gep);
        auto* dst_pos = b.CreateAdd(len_a, cur_j);
        auto* dst_b_gep = b.CreateGEP(i64_type, new_buf, dst_pos);
        b.CreateStore(val_b, dst_b_gep);
        b.CreateStore(b.CreateAdd(cur_j, llvm::ConstantInt::get(i64_type, 1)), i_alloc);
        b.CreateBr(loop_b_real_cond);

        b.SetInsertPoint(exit_bb);
        llvm::Value* ret_struct = llvm::UndefValue::get(array_type);
        ret_struct = b.CreateInsertValue(ret_struct, new_len, 0);
        ret_struct = b.CreateInsertValue(ret_struct, new_buf, 1);
        b.CreateRet(ret_struct);
    }

    // emojilang_array_push({i64, ptr}, i64)
    {
        auto* fn_ty = llvm::FunctionType::get(array_type, { array_type, i64_type }, false);
        array_push_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_array_push", module.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", array_push_fn);
        auto* loop_cond = llvm::BasicBlock::Create(ctx, "loop.cond", array_push_fn);
        auto* loop_body = llvm::BasicBlock::Create(ctx, "loop.body", array_push_fn);
        auto* exit_bb = llvm::BasicBlock::Create(ctx, "exit", array_push_fn);

        llvm::IRBuilder<> b(entry_bb);
        auto* len = b.CreateExtractValue(array_push_fn->getArg(0), 0);
        auto* data = b.CreateExtractValue(array_push_fn->getArg(0), 1);
        auto* val = array_push_fn->getArg(1);

        auto* new_len = b.CreateAdd(len, llvm::ConstantInt::get(i64_type, 1));
        auto* new_buf = b.CreateCall(calloc_fn, { new_len, llvm::ConstantInt::get(i64_type, 8) });

        auto* i_alloc = b.CreateAlloca(i64_type, nullptr, "i");
        b.CreateStore(llvm::ConstantInt::get(i64_type, 0), i_alloc);
        b.CreateBr(loop_cond);

        b.SetInsertPoint(loop_cond);
        auto* cur_i = b.CreateLoad(i64_type, i_alloc);
        auto* has_more = b.CreateICmpSLT(cur_i, len);
        b.CreateCondBr(has_more, loop_body, exit_bb);

        b.SetInsertPoint(loop_body);
        auto* src_gep = b.CreateGEP(i64_type, data, cur_i);
        auto* elem = b.CreateLoad(i64_type, src_gep);
        auto* dst_gep = b.CreateGEP(i64_type, new_buf, cur_i);
        b.CreateStore(elem, dst_gep);
        b.CreateStore(b.CreateAdd(cur_i, llvm::ConstantInt::get(i64_type, 1)), i_alloc);
        b.CreateBr(loop_cond);

        b.SetInsertPoint(exit_bb);
        auto* last_gep = b.CreateGEP(i64_type, new_buf, len);
        b.CreateStore(val, last_gep);
        llvm::Value* ret_struct = llvm::UndefValue::get(array_type);
        ret_struct = b.CreateInsertValue(ret_struct, new_len, 0);
        ret_struct = b.CreateInsertValue(ret_struct, new_buf, 1);
        b.CreateRet(ret_struct);
    }

    // emojilang_array_prepend(i64, {i64, ptr})
    {
        auto* fn_ty = llvm::FunctionType::get(array_type, { i64_type, array_type }, false);
        array_prepend_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_array_prepend", module.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", array_prepend_fn);
        auto* loop_cond = llvm::BasicBlock::Create(ctx, "loop.cond", array_prepend_fn);
        auto* loop_body = llvm::BasicBlock::Create(ctx, "loop.body", array_prepend_fn);
        auto* exit_bb = llvm::BasicBlock::Create(ctx, "exit", array_prepend_fn);

        llvm::IRBuilder<> b(entry_bb);
        auto* val = array_prepend_fn->getArg(0);
        auto* len = b.CreateExtractValue(array_prepend_fn->getArg(1), 0);
        auto* data = b.CreateExtractValue(array_prepend_fn->getArg(1), 1);

        auto* new_len = b.CreateAdd(len, llvm::ConstantInt::get(i64_type, 1));
        auto* new_buf = b.CreateCall(calloc_fn, { new_len, llvm::ConstantInt::get(i64_type, 8) });
        b.CreateStore(val, new_buf);

        auto* i_alloc = b.CreateAlloca(i64_type, nullptr, "i");
        b.CreateStore(llvm::ConstantInt::get(i64_type, 0), i_alloc);
        b.CreateBr(loop_cond);

        b.SetInsertPoint(loop_cond);
        auto* cur_i = b.CreateLoad(i64_type, i_alloc);
        auto* has_more = b.CreateICmpSLT(cur_i, len);
        b.CreateCondBr(has_more, loop_body, exit_bb);

        b.SetInsertPoint(loop_body);
        auto* src_gep = b.CreateGEP(i64_type, data, cur_i);
        auto* elem = b.CreateLoad(i64_type, src_gep);
        auto* dst_idx = b.CreateAdd(cur_i, llvm::ConstantInt::get(i64_type, 1));
        auto* dst_gep = b.CreateGEP(i64_type, new_buf, dst_idx);
        b.CreateStore(elem, dst_gep);
        b.CreateStore(b.CreateAdd(cur_i, llvm::ConstantInt::get(i64_type, 1)), i_alloc);
        b.CreateBr(loop_cond);

        b.SetInsertPoint(exit_bb);
        llvm::Value* ret_struct = llvm::UndefValue::get(array_type);
        ret_struct = b.CreateInsertValue(ret_struct, new_len, 0);
        ret_struct = b.CreateInsertValue(ret_struct, new_buf, 1);
        b.CreateRet(ret_struct);
    }

    // emojilang_array_eq({i64, ptr}, {i64, ptr})
    {
        auto* fn_ty = llvm::FunctionType::get(i64_type, { array_type, array_type }, false);
        array_eq_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_array_eq", module.get());
        auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", array_eq_fn);
        auto* loop_cond_bb = llvm::BasicBlock::Create(ctx, "loop.cond", array_eq_fn);
        auto* loop_body_bb = llvm::BasicBlock::Create(ctx, "loop.body", array_eq_fn);
        auto* loop_inc_bb = llvm::BasicBlock::Create(ctx, "loop.inc", array_eq_fn);
        auto* ret_false_bb = llvm::BasicBlock::Create(ctx, "ret.false", array_eq_fn);
        auto* ret_true_bb = llvm::BasicBlock::Create(ctx, "ret.true", array_eq_fn);

        llvm::IRBuilder<> b(entry_bb);
        auto* len_a = b.CreateExtractValue(array_eq_fn->getArg(0), 0);
        auto* data_a = b.CreateExtractValue(array_eq_fn->getArg(0), 1);
        auto* len_b = b.CreateExtractValue(array_eq_fn->getArg(1), 0);
        auto* data_b = b.CreateExtractValue(array_eq_fn->getArg(1), 1);

        auto* i_alloc = b.CreateAlloca(i64_type, nullptr, "i");
        b.CreateStore(llvm::ConstantInt::get(i64_type, 0), i_alloc);

        auto* lens_eq = b.CreateICmpEQ(len_a, len_b);
        b.CreateCondBr(lens_eq, loop_cond_bb, ret_false_bb);

        b.SetInsertPoint(loop_cond_bb);
        auto* cur_i = b.CreateLoad(i64_type, i_alloc);
        auto* has_more = b.CreateICmpSLT(cur_i, len_a);
        b.CreateCondBr(has_more, loop_body_bb, ret_true_bb);

        b.SetInsertPoint(loop_body_bb);
        auto* gep_a = b.CreateGEP(i64_type, data_a, cur_i);
        auto* val_a = b.CreateLoad(i64_type, gep_a);
        auto* gep_b = b.CreateGEP(i64_type, data_b, cur_i);
        auto* val_b = b.CreateLoad(i64_type, gep_b);
        auto* elem_eq = b.CreateICmpEQ(val_a, val_b);
        b.CreateCondBr(elem_eq, loop_inc_bb, ret_false_bb);

        b.SetInsertPoint(loop_inc_bb);
        b.CreateStore(b.CreateAdd(cur_i, llvm::ConstantInt::get(i64_type, 1)), i_alloc);
        b.CreateBr(loop_cond_bb);

        b.SetInsertPoint(ret_false_bb);
        b.CreateRet(llvm::ConstantInt::get(i64_type, 0));

        b.SetInsertPoint(ret_true_bb);
        b.CreateRet(llvm::ConstantInt::get(i64_type, 1));

        // emojilang_array_ne
        array_ne_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "emojilang_array_ne", module.get());
        auto* ne_bb = llvm::BasicBlock::Create(ctx, "entry", array_ne_fn);
        b.SetInsertPoint(ne_bb);
        auto* eq_res = b.CreateCall(array_eq_fn, { array_ne_fn->getArg(0), array_ne_fn->getArg(1) });
        auto* ne_res = b.CreateXor(eq_res, llvm::ConstantInt::get(i64_type, 1));
        b.CreateRet(ne_res);
    }
}

TypeKind CodeGen::infer_expr_type(const Expr& expr, const std::unordered_map<std::string, TypeKind>& local_types) {
    if (dynamic_cast<const NumExpr*>(&expr)) return TypeKind::Int;
    if (dynamic_cast<const ArrayLitExpr*>(&expr)) return TypeKind::Array;
    if (dynamic_cast<const LenExpr*>(&expr)) return TypeKind::Int;
    if (dynamic_cast<const IndexExpr*>(&expr)) return TypeKind::Int;
    if (dynamic_cast<const SliceExpr*>(&expr)) return TypeKind::Array;
    if (auto* var = dynamic_cast<const VarExpr*>(&expr)) {
        auto it = local_types.find(var->name);
        if (it != local_types.end()) return it->second;
        return TypeKind::Unknown;
    }
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = funcs.find(call->callee);
        if (it != funcs.end()) return it->second.ret_type;
        return TypeKind::Unknown;
    }
    if (auto* bin = dynamic_cast<const BinOpExpr*>(&expr)) {
        if (bin->op == "+") {
            TypeKind lt = infer_expr_type(*bin->left, local_types);
            TypeKind rt = infer_expr_type(*bin->right, local_types);
            if (lt == TypeKind::Array || rt == TypeKind::Array) return TypeKind::Array;
            if (lt == TypeKind::Int && rt == TypeKind::Int) return TypeKind::Int;
            return TypeKind::Unknown;
        }
        return TypeKind::Int;
    }
    return TypeKind::Unknown;
}

void CodeGen::infer_types(const Program& program) {
    // Collect all functions
    for (const auto& stmt : program.stmts) {
        if (auto* fn_def = dynamic_cast<const FuncDefStmt*>(stmt.get())) {
            FuncInfo fi;
            fi.name = fn_def->name;
            fi.param_types.resize(fn_def->params.size(), TypeKind::Unknown);
            fi.ret_type = TypeKind::Unknown;
            fi.ast_node = fn_def;
            funcs[fn_def->name] = fi;
        }
    }

    // Iterative constraint propagation
    bool changed = true;
    for (int iter = 0; iter < 10 && changed; ++iter) {
        changed = false;

        // Propagate within functions
        for (auto& pair : funcs) {
            FuncInfo& fi = pair.second;
            const FuncDefStmt* fn = fi.ast_node;

            std::unordered_map<std::string, TypeKind> local_types;
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (fi.param_types[i] != TypeKind::Unknown) {
                    local_types[fn->params[i]] = fi.param_types[i];
                }
            }

            // Helper lambda to scan AST and constrain types
            auto scan_expr = [&](auto& self, const Expr& expr) -> void {
                if (auto* len_expr = dynamic_cast<const LenExpr*>(&expr)) {
                    if (auto* v = dynamic_cast<const VarExpr*>(len_expr->arr.get())) {
                        if (local_types[v->name] != TypeKind::Array) {
                            local_types[v->name] = TypeKind::Array;
                            changed = true;
                        }
                    }
                    self(self, *len_expr->arr);
                } else if (auto* idx_expr = dynamic_cast<const IndexExpr*>(&expr)) {
                    if (auto* v = dynamic_cast<const VarExpr*>(idx_expr->arr.get())) {
                        if (local_types[v->name] != TypeKind::Array) {
                            local_types[v->name] = TypeKind::Array;
                            changed = true;
                        }
                    }
                    self(self, *idx_expr->arr);
                    self(self, *idx_expr->index);
                } else if (auto* sl_expr = dynamic_cast<const SliceExpr*>(&expr)) {
                    if (auto* v = dynamic_cast<const VarExpr*>(sl_expr->arr.get())) {
                        if (local_types[v->name] != TypeKind::Array) {
                            local_types[v->name] = TypeKind::Array;
                            changed = true;
                        }
                    }
                    self(self, *sl_expr->arr);
                    self(self, *sl_expr->start);
                    self(self, *sl_expr->end);
                } else if (auto* bin = dynamic_cast<const BinOpExpr*>(&expr)) {
                    self(self, *bin->left);
                    self(self, *bin->right);
                } else if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
                    auto it = funcs.find(call->callee);
                    if (it != funcs.end()) {
                        for (size_t a = 0; a < call->args.size() && a < it->second.param_types.size(); ++a) {
                            TypeKind arg_tk = infer_expr_type(*call->args[a], local_types);
                            if (arg_tk != TypeKind::Unknown && it->second.param_types[a] != arg_tk) {
                                it->second.param_types[a] = arg_tk;
                                changed = true;
                            }
                        }
                    }
                    for (const auto& a : call->args) self(self, *a);
                }
            };

            auto scan_stmt = [&](auto& self, const Stmt& s) -> void {
                if (auto* decl = dynamic_cast<const DeclStmt*>(&s)) {
                    scan_expr(scan_expr, *decl->init);
                    TypeKind tk = infer_expr_type(*decl->init, local_types);
                    if (tk != TypeKind::Unknown && local_types[decl->name] != tk) {
                        local_types[decl->name] = tk;
                        changed = true;
                    }
                } else if (auto* assign = dynamic_cast<const AssignStmt*>(&s)) {
                    scan_expr(scan_expr, *assign->val);
                    TypeKind tk = infer_expr_type(*assign->val, local_types);
                    if (assign->has_inbox && tk == TypeKind::Array) {
                        if (local_types[assign->name] != TypeKind::Array) {
                            local_types[assign->name] = TypeKind::Array;
                            changed = true;
                        }
                    } else if (tk != TypeKind::Unknown && local_types[assign->name] != tk) {
                        local_types[assign->name] = tk;
                        changed = true;
                    }
                } else if (auto* ret = dynamic_cast<const ReturnStmt*>(&s)) {
                    scan_expr(scan_expr, *ret->val);
                    TypeKind tk = infer_expr_type(*ret->val, local_types);
                    if (tk != TypeKind::Unknown && fi.ret_type != tk) {
                        fi.ret_type = tk;
                        changed = true;
                    }
                } else if (auto* if_s = dynamic_cast<const IfStmt*>(&s)) {
                    scan_expr(scan_expr, *if_s->cond);
                    for (const auto& st : if_s->then_block->stmts) self(self, *st);
                    if (if_s->else_block) {
                        for (const auto& st : if_s->else_block->stmts) self(self, *st);
                    }
                } else if (auto* wh_s = dynamic_cast<const WhileStmt*>(&s)) {
                    scan_expr(scan_expr, *wh_s->cond);
                    for (const auto& st : wh_s->body->stmts) self(self, *st);
                } else if (auto* pr_s = dynamic_cast<const PrintStmt*>(&s)) {
                    scan_expr(scan_expr, *pr_s->val);
                } else if (auto* ex_s = dynamic_cast<const ExprStmt*>(&s)) {
                    scan_expr(scan_expr, *ex_s->expr);
                }
            };

            for (const auto& s : fn->body->stmts) {
                scan_stmt(scan_stmt, *s);
            }

            // Sync parameter types back to fi
            for (size_t i = 0; i < fn->params.size(); ++i) {
                auto it = local_types.find(fn->params[i]);
                if (it != local_types.end() && fi.param_types[i] != it->second) {
                    fi.param_types[i] = it->second;
                    changed = true;
                }
            }
        }
    }

    // Default any remaining Unknown to Int
    for (auto& pair : funcs) {
        FuncInfo& fi = pair.second;
        if (fi.ret_type == TypeKind::Unknown) {
            fi.ret_type = TypeKind::Int;
        }
        for (size_t i = 0; i < fi.param_types.size(); ++i) {
            if (fi.param_types[i] == TypeKind::Unknown) {
                fi.param_types[i] = TypeKind::Int;
            }
        }
    }
}

void CodeGen::push_scope() {
    scopes.push_back({});
}

void CodeGen::pop_scope() {
    if (!scopes.empty()) {
        scopes.pop_back();
    }
}

VarInfo CodeGen::lookup_var(const std::string& name, int line) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    throw std::runtime_error("Line " + std::to_string(line) + ": Undefined variable: " + name);
}

llvm::AllocaInst* CodeGen::create_entry_block_alloca(llvm::Function* fn, const std::string& name, llvm::Type* type) {
    llvm::IRBuilder<> tmp_builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return tmp_builder.CreateAlloca(type, nullptr, name);
}

TypeKind CodeGen::get_expr_type(const Expr& expr) {
    if (dynamic_cast<const NumExpr*>(&expr)) return TypeKind::Int;
    if (dynamic_cast<const ArrayLitExpr*>(&expr)) return TypeKind::Array;
    if (dynamic_cast<const LenExpr*>(&expr)) return TypeKind::Int;
    if (dynamic_cast<const IndexExpr*>(&expr)) return TypeKind::Int;
    if (dynamic_cast<const SliceExpr*>(&expr)) return TypeKind::Array;
    if (auto* var = dynamic_cast<const VarExpr*>(&expr)) {
        return lookup_var(var->name, var->line).type_kind;
    }
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = funcs.find(call->callee);
        if (it != funcs.end()) return it->second.ret_type;
        return TypeKind::Int;
    }
    if (auto* bin = dynamic_cast<const BinOpExpr*>(&expr)) {
        if (bin->op == "+") {
            TypeKind lt = get_expr_type(*bin->left);
            TypeKind rt = get_expr_type(*bin->right);
            if (lt == TypeKind::Array || rt == TypeKind::Array) return TypeKind::Array;
            return TypeKind::Int;
        }
        return TypeKind::Int;
    }
    return TypeKind::Int;
}

void CodeGen::codegen_func_def(const FuncDefStmt& func_def) {
    FuncInfo& fi = funcs[func_def.name];

    std::vector<llvm::Type*> param_types;
    for (TypeKind tk : fi.param_types) {
        param_types.push_back((tk == TypeKind::Int) ? i64_type : array_type);
    }
    llvm::Type* ret_llvm_type = (fi.ret_type == TypeKind::Int) ? i64_type : array_type;

    auto* fn_type = llvm::FunctionType::get(ret_llvm_type, param_types, false);
    auto* fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, func_def.name, module.get());
    fi.llvm_fn = fn;

    size_t p_idx = 0;
    for (auto& arg : fn->args()) {
        arg.setName(func_def.params[p_idx++]);
    }

    auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    builder.SetInsertPoint(entry_bb);
    current_function = fn;

    push_scope();
    p_idx = 0;
    for (auto& arg : fn->args()) {
        std::string pname = func_def.params[p_idx];
        TypeKind ptk = fi.param_types[p_idx];
        llvm::Type* pty = (ptk == TypeKind::Int) ? i64_type : array_type;
        auto* alloc = create_entry_block_alloca(fn, pname, pty);
        builder.CreateStore(&arg, alloc);
        scopes.back()[pname] = { alloc, ptk };
        p_idx++;
    }

    for (const auto& s : func_def.body->stmts) {
        codegen_stmt(*s);
    }
    pop_scope();

    if (!builder.GetInsertBlock()->getTerminator()) {
        if (fi.ret_type == TypeKind::Int) {
            builder.CreateRet(llvm::ConstantInt::get(i64_type, 0));
        } else {
            builder.CreateRet(llvm::UndefValue::get(array_type));
        }
    }
}

void CodeGen::codegen_stmt(const Stmt& stmt) {
    if (builder.GetInsertBlock()->getTerminator()) {
        return; // Current basic block already terminated
    }

    if (auto* decl = dynamic_cast<const DeclStmt*>(&stmt)) {
        auto* val = codegen_expr(*decl->init);
        TypeKind tk = get_expr_type(*decl->init);
        llvm::Type* llvm_ty = (tk == TypeKind::Int) ? i64_type : array_type;
        auto* alloc = create_entry_block_alloca(current_function, decl->name, llvm_ty);
        builder.CreateStore(val, alloc);
        scopes.back()[decl->name] = { alloc, tk };
    } else if (auto* assign = dynamic_cast<const AssignStmt*>(&stmt)) {
        auto* val = codegen_expr(*assign->val);
        TypeKind val_tk = get_expr_type(*assign->val);
        VarInfo vi = lookup_var(assign->name, assign->line);

        if (assign->has_inbox && vi.type_kind == TypeKind::Array) {
            auto* old_arr = builder.CreateLoad(array_type, vi.alloca_inst, "old_arr");
            llvm::Value* new_arr = nullptr;
            if (val_tk == TypeKind::Array) {
                new_arr = builder.CreateCall(array_concat_fn, { old_arr, val }, "concat_arr");
            } else {
                new_arr = builder.CreateCall(array_push_fn, { old_arr, val }, "pushed_arr");
            }
            builder.CreateStore(new_arr, vi.alloca_inst);
        } else {
            builder.CreateStore(val, vi.alloca_inst);
        }
    } else if (auto* if_stmt = dynamic_cast<const IfStmt*>(&stmt)) {
        auto* cond_val = codegen_expr(*if_stmt->cond);
        auto* cond_bool = builder.CreateICmpNE(cond_val, llvm::ConstantInt::get(i64_type, 0), "ifcond");

        auto* then_bb = llvm::BasicBlock::Create(ctx, "then", current_function);
        auto* else_bb = llvm::BasicBlock::Create(ctx, "else", current_function);
        auto* merge_bb = llvm::BasicBlock::Create(ctx, "ifcont", current_function);

        builder.CreateCondBr(cond_bool, then_bb, else_bb);

        // Then block
        builder.SetInsertPoint(then_bb);
        push_scope();
        for (const auto& s : if_stmt->then_block->stmts) {
            codegen_stmt(*s);
        }
        pop_scope();
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(merge_bb);
        }

        // Else block
        builder.SetInsertPoint(else_bb);
        if (if_stmt->else_block) {
            push_scope();
            for (const auto& s : if_stmt->else_block->stmts) {
                codegen_stmt(*s);
            }
            pop_scope();
        }
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(merge_bb);
        }

        // Merge block
        if (merge_bb->hasNPredecessors(0)) {
            merge_bb->eraseFromParent();
        } else {
            builder.SetInsertPoint(merge_bb);
        }
    } else if (auto* while_stmt = dynamic_cast<const WhileStmt*>(&stmt)) {
        auto* cond_bb = llvm::BasicBlock::Create(ctx, "while.cond", current_function);
        auto* body_bb = llvm::BasicBlock::Create(ctx, "while.body", current_function);
        auto* exit_bb = llvm::BasicBlock::Create(ctx, "while.exit", current_function);

        builder.CreateBr(cond_bb);

        builder.SetInsertPoint(cond_bb);
        auto* cond_val = codegen_expr(*while_stmt->cond);
        auto* cond_bool = builder.CreateICmpNE(cond_val, llvm::ConstantInt::get(i64_type, 0), "while.cond.bool");
        builder.CreateCondBr(cond_bool, body_bb, exit_bb);

        builder.SetInsertPoint(body_bb);
        push_scope();
        for (const auto& s : while_stmt->body->stmts) {
            codegen_stmt(*s);
        }
        pop_scope();
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(cond_bb);
        }

        builder.SetInsertPoint(exit_bb);
    } else if (auto* ret_stmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
        auto* val = codegen_expr(*ret_stmt->val);
        builder.CreateRet(val);
        auto* dead_bb = llvm::BasicBlock::Create(ctx, "after.ret", current_function);
        builder.SetInsertPoint(dead_bb);
    } else if (auto* print_stmt = dynamic_cast<const PrintStmt*>(&stmt)) {
        auto* val = codegen_expr(*print_stmt->val);
        TypeKind tk = get_expr_type(*print_stmt->val);
        if (tk == TypeKind::Int) {
            builder.CreateCall(print_int_fn, { val });
        } else {
            builder.CreateCall(print_array_fn, { val });
        }
    } else if (auto* expr_stmt = dynamic_cast<const ExprStmt*>(&stmt)) {
        codegen_expr(*expr_stmt->expr);
    } else if (auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
        push_scope();
        for (const auto& s : block->stmts) {
            codegen_stmt(*s);
        }
        pop_scope();
    }
}

llvm::Value* CodeGen::codegen_expr(const Expr& expr) {
    if (auto* num = dynamic_cast<const NumExpr*>(&expr)) {
        return llvm::ConstantInt::get(i64_type, num->val);
    }
    if (auto* var = dynamic_cast<const VarExpr*>(&expr)) {
        VarInfo vi = lookup_var(var->name, var->line);
        llvm::Type* ty = (vi.type_kind == TypeKind::Int) ? i64_type : array_type;
        return builder.CreateLoad(ty, vi.alloca_inst, var->name);
    }
    if (auto* lit = dynamic_cast<const ArrayLitExpr*>(&expr)) {
        size_t n = lit->elements.size();
        auto* size_val = llvm::ConstantInt::get(i64_type, n);
        auto* buf = builder.CreateCall(calloc_fn, { size_val, llvm::ConstantInt::get(i64_type, 8) }, "arr.buf");

        for (size_t i = 0; i < n; ++i) {
            auto* elem = codegen_expr(*lit->elements[i]);
            auto* gep = builder.CreateGEP(i64_type, buf, llvm::ConstantInt::get(i64_type, i));
            builder.CreateStore(elem, gep);
        }

        llvm::Value* res_struct = llvm::UndefValue::get(array_type);
        res_struct = builder.CreateInsertValue(res_struct, size_val, 0);
        res_struct = builder.CreateInsertValue(res_struct, buf, 1);
        return res_struct;
    }
    if (auto* len_expr = dynamic_cast<const LenExpr*>(&expr)) {
        auto* arr_val = codegen_expr(*len_expr->arr);
        return builder.CreateExtractValue(arr_val, 0, "len");
    }
    if (auto* idx_expr = dynamic_cast<const IndexExpr*>(&expr)) {
        auto* arr_val = codegen_expr(*idx_expr->arr);
        auto* idx_val = codegen_expr(*idx_expr->index);
        auto* len = builder.CreateExtractValue(arr_val, 0, "arr.len");
        auto* data = builder.CreateExtractValue(arr_val, 1, "arr.data");

        builder.CreateCall(bounds_check_fn, { idx_val, len });
        auto* gep = builder.CreateGEP(i64_type, data, idx_val, "idx.gep");
        return builder.CreateLoad(i64_type, gep, "elem.val");
    }
    if (auto* sl_expr = dynamic_cast<const SliceExpr*>(&expr)) {
        auto* arr_val = codegen_expr(*sl_expr->arr);
        auto* start_val = codegen_expr(*sl_expr->start);
        auto* end_val = codegen_expr(*sl_expr->end);
        return builder.CreateCall(slice_fn, { arr_val, start_val, end_val }, "slice.res");
    }
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        auto it = funcs.find(call->callee);
        if (it == funcs.end() || !it->second.llvm_fn) {
            throw std::runtime_error("Line " + std::to_string(call->line) + ": Undefined function: " + call->callee);
        }
        std::vector<llvm::Value*> args;
        for (const auto& a : call->args) {
            args.push_back(codegen_expr(*a));
        }
        return builder.CreateCall(it->second.llvm_fn, args, call->callee + "_call");
    }
    if (auto* bin = dynamic_cast<const BinOpExpr*>(&expr)) {
        // Short-circuit logical operators
        if (bin->op == "&&") {
            auto* res_alloc = create_entry_block_alloca(current_function, "and_res", i64_type);
            builder.CreateStore(llvm::ConstantInt::get(i64_type, 0), res_alloc);

            auto* eval_right_bb = llvm::BasicBlock::Create(ctx, "and.rhs", current_function);
            auto* merge_bb = llvm::BasicBlock::Create(ctx, "and.end", current_function);

            auto* left_val = codegen_expr(*bin->left);
            auto* left_cond = builder.CreateICmpNE(left_val, llvm::ConstantInt::get(i64_type, 0));
            builder.CreateCondBr(left_cond, eval_right_bb, merge_bb);

            builder.SetInsertPoint(eval_right_bb);
            auto* right_val = codegen_expr(*bin->right);
            auto* right_cond = builder.CreateICmpNE(right_val, llvm::ConstantInt::get(i64_type, 0));
            auto* r_ext = builder.CreateZExt(right_cond, i64_type);
            builder.CreateStore(r_ext, res_alloc);
            builder.CreateBr(merge_bb);

            builder.SetInsertPoint(merge_bb);
            return builder.CreateLoad(i64_type, res_alloc);
        }
        if (bin->op == "||") {
            auto* res_alloc = create_entry_block_alloca(current_function, "or_res", i64_type);
            builder.CreateStore(llvm::ConstantInt::get(i64_type, 1), res_alloc);

            auto* eval_right_bb = llvm::BasicBlock::Create(ctx, "or.rhs", current_function);
            auto* merge_bb = llvm::BasicBlock::Create(ctx, "or.end", current_function);

            auto* left_val = codegen_expr(*bin->left);
            auto* left_cond = builder.CreateICmpNE(left_val, llvm::ConstantInt::get(i64_type, 0));
            builder.CreateCondBr(left_cond, merge_bb, eval_right_bb);

            builder.SetInsertPoint(eval_right_bb);
            auto* right_val = codegen_expr(*bin->right);
            auto* right_cond = builder.CreateICmpNE(right_val, llvm::ConstantInt::get(i64_type, 0));
            auto* r_ext = builder.CreateZExt(right_cond, i64_type);
            builder.CreateStore(r_ext, res_alloc);
            builder.CreateBr(merge_bb);

            builder.SetInsertPoint(merge_bb);
            return builder.CreateLoad(i64_type, res_alloc);
        }

        auto* left_val = codegen_expr(*bin->left);
        auto* right_val = codegen_expr(*bin->right);
        TypeKind lt = get_expr_type(*bin->left);
        TypeKind rt = get_expr_type(*bin->right);

        if (bin->op == "+") {
            if (lt == TypeKind::Int && rt == TypeKind::Int) {
                return builder.CreateAdd(left_val, right_val, "add");
            }
            if (lt == TypeKind::Array && rt == TypeKind::Array) {
                return builder.CreateCall(array_concat_fn, { left_val, right_val }, "concat");
            }
            if (lt == TypeKind::Array && rt == TypeKind::Int) {
                return builder.CreateCall(array_push_fn, { left_val, right_val }, "push");
            }
            if (lt == TypeKind::Int && rt == TypeKind::Array) {
                return builder.CreateCall(array_prepend_fn, { left_val, right_val }, "prepend");
            }
        }
        if (bin->op == "-") return builder.CreateSub(left_val, right_val, "sub");
        if (bin->op == "*") return builder.CreateMul(left_val, right_val, "mul");
        if (bin->op == "/") return builder.CreateSDiv(left_val, right_val, "sdiv");
        if (bin->op == "%") return builder.CreateSRem(left_val, right_val, "srem");

        if (bin->op == "<") {
            auto* cmp = builder.CreateICmpSLT(left_val, right_val, "cmp.lt");
            return builder.CreateZExt(cmp, i64_type);
        }
        if (bin->op == ">") {
            auto* cmp = builder.CreateICmpSGT(left_val, right_val, "cmp.gt");
            return builder.CreateZExt(cmp, i64_type);
        }
        if (bin->op == "<=") {
            auto* cmp = builder.CreateICmpSLE(left_val, right_val, "cmp.le");
            return builder.CreateZExt(cmp, i64_type);
        }
        if (bin->op == ">=") {
            auto* cmp = builder.CreateICmpSGE(left_val, right_val, "cmp.ge");
            return builder.CreateZExt(cmp, i64_type);
        }
        if (bin->op == "==") {
            if (lt == TypeKind::Array && rt == TypeKind::Array) {
                return builder.CreateCall(array_eq_fn, { left_val, right_val }, "arr.eq");
            }
            auto* cmp = builder.CreateICmpEQ(left_val, right_val, "cmp.eq");
            return builder.CreateZExt(cmp, i64_type);
        }
        if (bin->op == "!=") {
            if (lt == TypeKind::Array && rt == TypeKind::Array) {
                return builder.CreateCall(array_ne_fn, { left_val, right_val }, "arr.ne");
            }
            auto* cmp = builder.CreateICmpNE(left_val, right_val, "cmp.ne");
            return builder.CreateZExt(cmp, i64_type);
        }

        throw std::runtime_error("Line " + std::to_string(bin->line) + ": Unknown operator: " + bin->op);
    }

    throw std::runtime_error("Line " + std::to_string(expr.line) + ": Unknown expression");
}

void CodeGen::generate(const Program& program) {
    infer_types(program);

    // First pass: generate all user functions
    for (const auto& stmt : program.stmts) {
        if (auto* fn_def = dynamic_cast<const FuncDefStmt*>(stmt.get())) {
            codegen_func_def(*fn_def);
        }
    }

    // Second pass: generate main() wrapping top-level statements
    auto* main_fn_type = llvm::FunctionType::get(i32_type, false);
    auto* main_fn = llvm::Function::Create(main_fn_type, llvm::Function::ExternalLinkage, "main", module.get());
    auto* entry_bb = llvm::BasicBlock::Create(ctx, "entry", main_fn);
    builder.SetInsertPoint(entry_bb);
    current_function = main_fn;

    push_scope();
    for (const auto& stmt : program.stmts) {
        if (!dynamic_cast<const FuncDefStmt*>(stmt.get())) {
            codegen_stmt(*stmt);
        }
    }
    pop_scope();

    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
    }

    // Verify module
    std::string err;
    llvm::raw_string_ostream err_os(err);
    if (llvm::verifyModule(*module, &err_os)) {
        throw std::runtime_error("LLVM module verification failed:\n" + err);
    }
}

void CodeGen::dump_ir(std::ostream& os) {
    std::string s;
    llvm::raw_string_ostream ros(s);
    module->print(ros, nullptr);
    os << ros.str();
}

void CodeGen::print_ir() {
    module->print(llvm::outs(), nullptr);
}

bool CodeGen::emit_object(const std::string& obj_path) {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto target_triple = llvm::sys::getDefaultTargetTriple();
    module->setTargetTriple(target_triple);

    std::string err;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, err);
    if (!target) {
        throw std::runtime_error("LLVM target lookup failed: " + err);
    }

    llvm::TargetOptions opt;
    auto rm = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
    auto* target_machine = target->createTargetMachine(target_triple, "generic", "", opt, rm);
    module->setDataLayout(target_machine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(obj_path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        throw std::runtime_error("Could not open object file for writing: " + ec.message());
    }

    llvm::legacy::PassManager pass;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        throw std::runtime_error("TargetMachine cannot emit object file");
    }

    pass.run(*module);
    dest.flush();
    return true;
}

bool CodeGen::emit_native(const std::string& exe_path) {
    char tmp_obj[] = "/tmp/emojilang_obj_XXXXXX.o";
    int fd = mkstemps(tmp_obj, 2);
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary object file");
    }
    close(fd);

    emit_object(tmp_obj);

    std::string cmd = "clang " + std::string(tmp_obj) + " -o " + exe_path;
    int ret = std::system(cmd.c_str());
    std::remove(tmp_obj);

    if (ret != 0) {
        throw std::runtime_error("Clang linking failed with code " + std::to_string(ret));
    }
    return true;
}
