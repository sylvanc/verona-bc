#include "../codegen.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace vbcc
{
  namespace llvm_backend
  {
    using TransferValue = std::function<std::optional<LoweredValue>(
      const Node& use, const Node& src)>;

    std::optional<std::vector<LoweredValue>> lower_args(
      const Node& args,
      const TransferValue& move_value,
      const TransferValue& copy_value);

    bool LLVMCodegen::emit_call(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto function_id = statement / FunctionId;
      auto args = statement / Args;
      auto function_name = node_text(function_id);
      auto function = functions.find(function_name);

      if (function == functions.end())
      {
        fail(statement, "call of unknown function '" + function_name + "'");
        return false;
      }

      auto& callee = function->second;

      if (args->size() != callee.param_types.size())
      {
        fail(statement, "wrong number of LLVM call arguments");
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

      std::vector<llvm::Value*> llvm_args;
      llvm_args.reserve(args->size());

      size_t i = 0;

      for (const auto& arg : *args)
      {
        auto& value = lowered_args->at(i);

        if (value.type != callee.param_types.at(i))
        {
          fail(arg, "call argument representation mismatch");
          return false;
        }

        if (value.value == nullptr)
        {
          fail(arg, "call argument has no runtime representation");
          return false;
        }

        llvm_args.push_back(value.value);
        ++i;
      }

      auto result_name = callee.return_type.kind == ValueKind::None ?
        std::string() :
        strip_sigil(node_text(dst));
      auto* call = builder.CreateCall(callee.function, llvm_args, result_name);
      call->setCallingConv(callee.function->getCallingConv());

      if (callee.return_type.kind == ValueKind::None)
      {
        return locals.bind_value(
          statement, dst, LoweredValue{callee.return_type, nullptr});
      }

      return locals.bind_value(
        statement, dst, LoweredValue{callee.return_type, call});
    }
  }
}
