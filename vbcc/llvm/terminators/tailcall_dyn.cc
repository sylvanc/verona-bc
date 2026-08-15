#include "../codegen.h"

#include <cassert>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <vector>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_tailcall_dyn(
      const Node& statement, const LoweredType& return_type)
    {
      auto move_args = statement / MoveArgs;
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

      auto target_id = statement / LocalId;
      auto target = locals.move_value(statement, target_id);

      if (!target)
        return false;

      if (
        (target->type.kind != ValueKind::Pointer) || (target->value == nullptr))
      {
        fail(statement, "dynamic tailcall target representation is not ptr");
        return false;
      }

      std::vector<llvm::Type*> param_types;
      std::vector<llvm::Value*> llvm_args;
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      param_types.reserve(args.size());
      llvm_args.reserve(args.size());

      for (const auto& arg : args)
      {
        if ((arg.type.kind == ValueKind::None) || (arg.value == nullptr))
        {
          fail(
            statement,
            "dynamic tailcall argument has no runtime representation");
          return false;
        }

        param_types.push_back(arg.type.value_type);
        llvm_args.push_back(arg.value);
      }

      auto* unknown_descriptor = llvm::ConstantPointerNull::get(pointer_type);

      if (!emit_prepare_tailcall(statement, unknown_descriptor))
        return false;

      auto* function_type =
        llvm::FunctionType::get(return_type.value_type, param_types, false);
      auto* call = builder.CreateCall(function_type, target->value, llvm_args);
      call->setCallingConv(llvm::CallingConv::Tail);
      call->setTailCallKind(llvm::CallInst::TCK_MustTail);

      if (return_type.kind == ValueKind::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(call);

      return true;
    }
  }
}
