#include "../codegen.h"

#include <cassert>
#include <cstddef>
#include <llvm/IR/Instructions.h>
#include <vector>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_tailcall(
      const Node& statement, const LoweredType& return_type)
    {
      auto function_id = statement / FunctionId;
      auto function = functions.find(node_text(function_id));

      if (function == functions.end())
      {
        fail(
          statement,
          "tailcall of unknown function '" + node_text(function_id) + "'");
        return false;
      }

      auto& callee = function->second;
      auto move_args = statement / MoveArgs;

      if (callee.return_type != return_type)
      {
        fail(statement, "tailcall return representation mismatch");
        return false;
      }

      if (move_args->size() != callee.param_types.size())
      {
        fail(statement, "wrong number of LLVM tailcall arguments");
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

      std::vector<llvm::Value*> llvm_args;
      llvm_args.reserve(args.size() + 2);
      llvm_args.push_back(current_thread);
      llvm_args.push_back(current_frame);

      for (size_t i = 0; i < args.size(); ++i)
      {
        auto& arg = args.at(i);

        if ((arg.type.kind == ValueKind::None) || (arg.value == nullptr))
        {
          fail(move_args, "tailcall argument has no runtime representation");
          return false;
        }

        if (arg.type != callee.param_types.at(i))
        {
          fail(move_args, "tailcall argument representation mismatch");
          return false;
        }

        llvm_args.push_back(arg.value);
      }

      if (!emit_prepare_tailcall(statement, callee.descriptor))
        return false;

      auto* call = builder.CreateCall(callee.function, llvm_args);
      call->setCallingConv(callee.function->getCallingConv());
      call->setTailCallKind(llvm::CallInst::TCK_MustTail);

      if (return_type.kind == ValueKind::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(call);

      return true;
    }
  }
}
