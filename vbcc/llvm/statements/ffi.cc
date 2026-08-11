#include "../codegen.h"

#include <cassert>
#include <cstddef>
#include <vector>

namespace vbcc
{
  namespace llvm_backend
  {
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

      std::vector<llvm::Value*> llvm_args;
      llvm_args.reserve(args->size());

      size_t i = 0;

      for (const auto& arg : *args)
      {
        auto arg_kind = arg / Type;
        auto src = arg / Rhs;
        std::optional<LoweredValue> value;

        if (arg_kind == ArgCopy)
        {
          auto* source = locals.find_value(src);

          if (!source)
          {
            fail(arg, "FFI ArgCopy of unknown local '" + node_text(src) + "'");
            return false;
          }

          value = *source;

          // ArgCopy has the same ownership semantics as Copy. Future managed
          // representations will emit their retain operation here.
          switch (value->type.ownership)
          {
            case OwnershipKind::Trivial:
              break;

            case OwnershipKind::Managed:
              fail(arg, "FFI argument cannot copy a managed value yet");
              return false;
          }
        }
        else
        {
          assert(arg_kind == ArgMove);
          value = locals.take_value(src);

          if (!value)
          {
            fail(arg, "FFI ArgMove of unknown local '" + node_text(src) + "'");
            return false;
          }
        }

        if (value->type != symbol.param_types.at(i))
        {
          fail(arg, "FFI argument representation mismatch");
          return false;
        }

        assert(value->value != nullptr);
        llvm_args.push_back(value->value);
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
