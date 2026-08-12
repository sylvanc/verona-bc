#include "../codegen.h"

#include <cassert>
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

    bool LLVMCodegen::emit_ffi(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto symbol_id = statement / SymbolId;
      auto args = statement / Args;
      auto symbol_name = node_text(symbol_id);
      auto symbol_it = symbols.find(symbol_name);

      if (symbol_it == symbols.end())
      {
        fail(statement, "unknown FFI symbol '" + symbol_name + "'");
        return false;
      }

      auto& symbol = symbol_it->second;
      assert(symbol.library_index < libraries.size());

      if (symbol.function == nullptr)
      {
        fail(statement, "dynamically resolved FFI symbols are not supported");
        return false;
      }

      if (args->size() != symbol.param_types.size())
      {
        fail(statement, "wrong number of LLVM FFI arguments");
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

        if (value.type != symbol.param_types.at(i))
        {
          fail(arg, "FFI argument representation mismatch");
          return false;
        }

        if (value.value == nullptr)
        {
          fail(arg, "FFI argument has no runtime representation");
          return false;
        }

        llvm_args.push_back(value.value);
        ++i;
      }

      auto dst_name = node_text(dst);
      auto result_name = symbol.return_type.kind == ValueKind::None ?
        std::string() :
        strip_sigil(dst_name);
      auto* call = builder.CreateCall(symbol.function, llvm_args, result_name);
      call->setCallingConv(symbol.function->getCallingConv());

      if (symbol.return_type.kind == ValueKind::None)
      {
        return locals.bind_value(
          statement, dst, LoweredValue{symbol.return_type, nullptr});
      }

      return locals.bind_value(
        statement, dst, LoweredValue{symbol.return_type, call});
    }
  }
}
