#include "codegen.h"

#include <cassert>
#include <cstddef>
#include <vector>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_ffi(const Node& statement)
    {
      auto symbol_name = node_text(statement / SymbolId);
      auto symbol_it = symbols.find(symbol_name);

      if (symbol_it == symbols.end())
      {
        fail(statement, "unknown FFI symbol '" + symbol_name + "'");
        return false;
      }

      auto& symbol = symbol_it->second;
      auto args_node = statement / Args;

      if (args_node->size() != symbol.signature.param_types.size())
      {
        fail(statement, "wrong number of LLVM FFI arguments");
        return false;
      }

      std::vector<llvm::Value*> args;
      args.reserve(args_node->size());

      size_t i = 0;

      for (const auto& arg : *args_node)
      {
        auto transfer =
          (arg / Type) == ArgMove ? TransferKind::Move : TransferKind::Copy;
        auto local = transfer_local(arg, arg / Rhs, transfer, "FFI argument");

        if (!local)
          return false;

        if (!same_value_representation(
              local->type, symbol.signature.param_types.at(i)))
        {
          fail(arg, "FFI argument representation mismatch");
          return false;
        }

        assert(local->value != nullptr);
        args.push_back(local->value);
        ++i;
      }

      auto dst = node_text(statement / LocalId);
      auto result_name = symbol.signature.return_type.kind == ValueKind::None ?
        std::string() :
        strip_sigil(dst);
      auto* call = builder.CreateCall(symbol.function, args, result_name);
      call->setCallingConv(symbol.signature.calling_convention);

      if (symbol.signature.return_type.kind == ValueKind::None)
      {
        return bind_local(
          statement,
          statement / LocalId,
          LoweredValue{symbol.signature.return_type, nullptr});
      }

      return bind_local(
        statement,
        statement / LocalId,
        LoweredValue{symbol.signature.return_type, call});
    }
  }
}
