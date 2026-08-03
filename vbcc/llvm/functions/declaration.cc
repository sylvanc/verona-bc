#include "../codegen.h"

#include <llvm/IR/Function.h>
#include <utility>

namespace vbcc
{
  namespace llvm_backend
  {
    void LLVMCodegen::declare_functions()
    {
      const auto function_name = [](const Node& id) {
        auto name = node_text(id);

        if (name == "@main")
          return std::string("verona_main");

        return "verona_fn_" + strip_sigil(name);
      };

      for (const auto& func_state : state.functions)
      {
        auto func = func_state.func;

        if (!func)
          continue;

        auto return_type = func / Type;
        auto params = func / Params;
        auto function_id = func / FunctionId;
        auto lowered_return = lower_type(return_type);
        auto lowered_params = lower_params(params);

        if (!lowered_return || !lowered_params)
          continue;

        std::vector<llvm::Type*> llvm_params;
        llvm_params.reserve(lowered_params->size());

        for (const auto& param_type : *lowered_params)
          llvm_params.push_back(param_type.value_type);

        auto* function_type = llvm::FunctionType::get(
          lowered_return->value_type, llvm_params, false);

        auto id = node_text(function_id);

        if (
          (id == "@main") &&
          ((lowered_return->kind != ValueKind::None) ||
           !lowered_params->empty()))
        {
          fail(func, "main must have the native signature @main(): none");
          continue;
        }

        auto name = function_name(function_id);

        if (module.getFunction(name))
        {
          fail(func, "duplicate LLVM function name '" + name + "'");
          continue;
        }

        auto linkage = id == "@main" ? llvm::GlobalValue::ExternalLinkage :
                                       llvm::GlobalValue::InternalLinkage;
        auto* function =
          llvm::Function::Create(function_type, linkage, name, module);

        function->setCallingConv(llvm::CallingConv::C);
        functions.emplace(
          id,
          LoweredFunction{
            function, *lowered_return, std::move(*lowered_params)});
      }
    }
  }
}
