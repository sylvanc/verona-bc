#include "../codegen.h"

#include <llvm/IR/Function.h>
#include <utility>

namespace vbcc
{
  namespace llvm_backend
  {
    void LLVMCodegen::declare_libraries()
    {
      const auto declare_symbol = [this](const Node& symbol) {
        auto linker_name = symbol / Lhs;
        auto symbol_id = symbol / SymbolId;
        auto version = symbol / Rhs;
        auto vararg = symbol / Vararg;
        auto return_type = symbol / Return;
        auto params = symbol / FFIParams;

        if (!node_text(version).empty())
        {
          fail(symbol, "versioned FFI symbols are not supported");
          return;
        }

        if (vararg == Vararg)
        {
          fail(symbol, "variadic FFI symbols are not supported");
          return;
        }

        auto lowered_return = lower_type(return_type);
        auto lowered_params = lower_params(params);

        if (!lowered_return || !lowered_params)
          return;

        std::vector<llvm::Type*> llvm_params;
        llvm_params.reserve(lowered_params->size());

        for (const auto& param_type : *lowered_params)
          llvm_params.push_back(param_type.value_type);

        auto* function_type = llvm::FunctionType::get(
          lowered_return->value_type, llvm_params, false);

        auto linker_name_text = node_text(linker_name);
        auto* function = module.getFunction(linker_name_text);

        if (function && (function->getFunctionType() != function_type))
        {
          fail(
            symbol,
            "conflicting declarations for FFI symbol '" + linker_name_text +
              "'");
          return;
        }

        if (!function)
        {
          function = llvm::Function::Create(
            function_type,
            llvm::GlobalValue::ExternalLinkage,
            linker_name_text,
            module);
        }

        function->setCallingConv(llvm::CallingConv::C);
        ffi_symbols.emplace(
          node_text(symbol_id),
          LoweredFunction{
            function, *lowered_return, std::move(*lowered_params)});
      };

      for (const auto& library : state.libraries)
      {
        auto library_name = node_text(library / String);

        if (!library_name.empty())
        {
          fail(library, "only process-local FFI libraries are supported");
          continue;
        }

        // assignids provides a deduplicated symbol index. Match its entries
        // back to their owning Lib so multiple VIR Lib fragments with the same
        // name retain the bytecode backend's symbol-merging behaviour.
        for (const auto& symbol : state.symbols)
        {
          auto owner = symbol->parent(Lib);

          if (!owner || (node_text(owner / String) != library_name))
            continue;

          declare_symbol(symbol);
        }
      }
    }
  }
}
