#include "../codegen.h"

#include <cassert>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <vector>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_tailcall_dyn(
      const Node& statement, const LoweredType& return_type)
    {
      auto target_id = statement / LocalId;
      auto borrowed_target = locals.find_value(target_id);

      if (
        !borrowed_target ||
        (borrowed_target->type.ir_type != IRValueType::Function) ||
        !borrowed_target->signature)
      {
        fail(statement, "dynamic tailcall target is not callable");
        return false;
      }

      const auto& borrowed_signature = *borrowed_target->signature;
      auto move_args = statement / MoveArgs;

      if (move_args->size() != borrowed_signature.param_types.size())
      {
        fail(statement, "wrong number of LLVM dynamic tailcall arguments");
        return false;
      }

      std::vector<LoweredValue> args;
      args.reserve(move_args->size());

      for (const auto& arg : *move_args)
      {
        assert(arg->type() == MoveArg);
        assert((arg / Type)->type() == ArgMove);
        auto value = locals.move_value(arg, arg / Rhs);

        if (!value)
          return false;

        args.push_back(*value);
      }

      auto target = locals.move_value(statement, target_id);

      if (!target)
        return false;

      if (
        (target->type.ir_type != IRValueType::Function) ||
        (target->value == nullptr) || !target->signature)
      {
        fail(statement, "dynamic tailcall target is not callable");
        return false;
      }

      const auto& signature = *target->signature;

      if (signature.return_type != return_type)
      {
        fail(statement, "dynamic tailcall return representation mismatch");
        return false;
      }

      std::vector<llvm::Type*> param_types;
      std::vector<llvm::Value*> llvm_args;
      param_types.reserve(args.size());
      llvm_args.reserve(args.size());

      for (std::size_t index = 0; index < args.size(); ++index)
      {
        const auto& arg = args.at(index);

        if ((arg.type.ir_type == IRValueType::None) || (arg.value == nullptr))
        {
          fail(
            statement,
            "dynamic tailcall argument has no runtime representation");
          return false;
        }

        if (arg.type != signature.param_types.at(index))
        {
          fail(
            move_args->at(index),
            "dynamic tailcall argument representation mismatch");
          return false;
        }

        if (!emit_validate_tailcall(move_args->at(index), arg))
          return false;

        param_types.push_back(arg.type.llvm_type);
        llvm_args.push_back(arg.value);
      }

      auto function_pointer = emit_callable_entry(statement, *target);

      if (!function_pointer)
        return false;

      if (!emit_reuse_frame(statement, target->value))
        return false;

      auto* function_type = llvm::FunctionType::get(
        signature.return_type.llvm_type, param_types, false);
      auto* call =
        builder.CreateCall(function_type, *function_pointer, llvm_args);
      call->setCallingConv(llvm::CallingConv::Tail);
      call->setTailCallKind(llvm::CallInst::TCK_MustTail);

      if (return_type.ir_type == IRValueType::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(call);

      return true;
    }
  }
}
