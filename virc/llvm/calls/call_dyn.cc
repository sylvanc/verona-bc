#include "../codegen.h"

#include <functional>
#include <llvm/IR/DerivedTypes.h>
#include <optional>
#include <vector>

namespace virc
{
  namespace llvm_backend
  {
    namespace
    {
      using DynamicTransferValue = std::function<std::optional<LoweredValue>(
        const Node& use, const Node& src)>;
    }

    std::optional<std::vector<LoweredValue>> lower_args(
      const Node& args,
      const DynamicTransferValue& move_value,
      const DynamicTransferValue& copy_value);

    bool LLVMCodegen::emit_call_dyn(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto target = locals.find_value(statement / Rhs);

      if (!target)
      {
        fail(
          statement,
          "dynamic call of unknown local '" + node_text(statement / Rhs) + "'");
        return false;
      }

      if ((target->type.ir_type != IRValueType::Function) || !target->signature)
      {
        fail(statement, "dynamic call target is not callable");
        return false;
      }

      auto args = statement / Args;
      const auto& signature = *target->signature;

      if (args->size() != signature.param_types.size())
      {
        fail(statement, "wrong number of LLVM dynamic call arguments");
        return false;
      }

      auto lowered_args = lower_args(
        args,
        [this](const Node& use, const Node& src) {
          return locals.move_value(use, src);
        },
        [this](const Node& use, const Node& src) {
          return locals.copy_value(use, src);
        });

      if (!lowered_args)
        return false;

      std::vector<llvm::Type*> param_types;
      std::vector<llvm::Value*> llvm_args;
      param_types.reserve(lowered_args->size());
      llvm_args.reserve(lowered_args->size());

      for (std::size_t index = 0; index < lowered_args->size(); ++index)
      {
        const auto& value = lowered_args->at(index);

        if (value.type != signature.param_types.at(index))
        {
          fail(
            args->at(index), "dynamic call argument representation mismatch");
          return false;
        }

        if (value.value == nullptr)
        {
          fail(args->at(index), "dynamic call argument has no representation");
          return false;
        }

        param_types.push_back(value.type.llvm_type);
        llvm_args.push_back(value.value);
      }

      auto function_pointer = emit_callable_entry(statement, *target);

      if (!function_pointer)
        return false;

      if (!emit_enter_frame(statement, target->value))
        return false;

      auto* function_type = llvm::FunctionType::get(
        signature.return_type.llvm_type, param_types, false);
      auto result_name = signature.return_type.ir_type == IRValueType::None ?
        std::string() :
        strip_sigil(node_text(dst));
      auto* call = builder.CreateCall(
        function_type, *function_pointer, llvm_args, result_name);
      call->setCallingConv(llvm::CallingConv::Tail);

      if (signature.return_type.ir_type == IRValueType::None)
      {
        return locals.bind_value(
          statement, dst, LoweredValue{signature.return_type, nullptr});
      }

      return locals.bind_value(
        statement, dst, LoweredValue{signature.return_type, call});
    }
  }
}
