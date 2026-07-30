#include "codegen.h"

#include <llvm/IR/Function.h>
#include <utility>

namespace vbcc
{
  namespace llvm_backend
  {
    /* declaration phase */
    void LLVMCodegen::declare_symbols()
    {
      for (const auto& symbol : state.symbols)
      {
        auto lib = symbol->parent(Lib);

        if (!lib || !node_text(lib / String).empty())
        {
          fail(symbol, "only process-local FFI libraries are supported");
          continue;
        }

        if (!node_text(symbol / Rhs).empty())
        {
          fail(symbol, "versioned FFI symbols are not supported");
          continue;
        }

        if ((symbol / Vararg) == Vararg)
        {
          fail(symbol, "variadic FFI symbols are not supported");
          continue;
        }

        auto signature = lower_signature(symbol / Return, symbol / FFIParams);

        if (!signature)
          continue;

        auto linker_name = node_text(symbol / Lhs);
        auto* function = module.getFunction(linker_name);

        if (
          function && (function->getFunctionType() != signature->function_type))
        {
          fail(
            symbol,
            "conflicting declarations for FFI symbol '" + linker_name + "'");
          continue;
        }

        if (!function)
        {
          function = llvm::Function::Create(
            signature->function_type,
            llvm::GlobalValue::ExternalLinkage,
            linker_name,
            module);
        }

        function->setCallingConv(signature->calling_convention);
        symbols.emplace(
          node_text(symbol / SymbolId),
          LoweredFunction{function, std::move(*signature)});
      }
    }

    void LLVMCodegen::declare_functions()
    {
      for (const auto& func_state : state.functions)
      {
        auto func = func_state.func;

        if (!func)
          continue;

        auto signature = lower_signature(func / Type, func / Params);

        if (!signature)
          continue;

        auto id = node_text(func / FunctionId);

        if (
          (id == "@main") &&
          ((signature->return_type.kind != ValueKind::None) ||
           !signature->param_types.empty()))
        {
          fail(func, "main must have the native signature @main(): none");
          continue;
        }

        auto name = function_name(func / FunctionId);

        if (module.getFunction(name))
        {
          fail(func, "duplicate LLVM function name '" + name + "'");
          continue;
        }

        auto linkage = id == "@main" ? llvm::GlobalValue::ExternalLinkage :
                                       llvm::GlobalValue::InternalLinkage;
        auto* function = llvm::Function::Create(
          signature->function_type, linkage, name, module);

        function->setCallingConv(signature->calling_convention);
        functions.emplace(id, LoweredFunction{function, std::move(*signature)});
      }
    }
  }
}
