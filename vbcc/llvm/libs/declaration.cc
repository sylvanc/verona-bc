#include "../codegen.h"

#include <cassert>
#include <llvm/IR/Function.h>
#include <utility>

namespace vbcc
{
  namespace llvm_backend
  {
    void LLVMCodegen::declare_libraries()
    {
      const auto declare_symbol =
        [this](const Node& symbol, size_t library_index) {
          auto linker_name = symbol / Lhs;
          auto symbol_id = symbol / SymbolId;
          auto version = symbol / Rhs;
          auto vararg = symbol / Vararg;
          auto return_type = symbol / Return;
          auto params = symbol / FFIParams;
          auto linker_name_text = node_text(linker_name);
          auto version_text = node_text(version);
          auto is_vararg = vararg == Vararg;

          if (!version_text.empty())
          {
            fail(symbol, "versioned FFI symbols are not supported");
            return;
          }

          if (is_vararg)
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
          symbols.emplace(
            node_text(symbol_id),
            LoweredSymbol{
              library_index,
              std::move(linker_name_text),
              std::move(version_text),
              is_vararg,
              *lowered_return,
              std::move(*lowered_params),
              function,
              nullptr});
        };

      std::unordered_map<std::string, size_t> library_indices;
      libraries.reserve(state.libraries.size());

      for (const auto& library : state.libraries)
      {
        auto path = node_text(library / String);
        auto init_func = library / InitFunc;
        auto library_index = libraries.size();
        auto insertion = library_indices.emplace(path, library_index);
        assert(insertion.second);

        libraries.push_back(
          LoweredLibrary{
            path,
            init_func->type() == FunctionId ?
              std::make_optional(node_text(init_func)) :
              std::nullopt,
            {}});

        if (!path.empty())
          fail(library, "only process-local FFI libraries are supported");
      }

      for (const auto& symbol : state.symbols)
      {
        auto owner = symbol->parent(Lib);
        assert(owner);

        auto path = node_text(owner / String);
        auto library_it = library_indices.find(path);
        assert(library_it != library_indices.end());

        auto library_index = library_it->second;
        auto symbol_id = node_text(symbol / SymbolId);
        libraries.at(library_index).symbol_ids.push_back(symbol_id);

        // The model records named-library membership now, but emitting its
        // runtime loader and indirect symbol slots remains future work.
        if (!libraries.at(library_index).path.empty())
          continue;

        declare_symbol(symbol, library_index);
      }
    }
  }
}
