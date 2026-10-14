#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    std::optional<llvm::Value*> LLVMCodegen::emit_callable_entry(
      const Node& statement, const LoweredValue& callable)
    {
      if (
        (callable.type.ir_type != IRValueType::Function) ||
        (callable.type.runtime_type != vrt::ValueType::raw_pointer) ||
        (callable.value == nullptr) || !callable.signature)
      {
        fail(statement, "dynamic call target is not a callable descriptor");
        return {};
      }

      if (runtime.func_entry == nullptr)
      {
        fail(statement, "LLVM callable runtime is unavailable");
        return {};
      }

      return builder.CreateCall(
        runtime.func_entry, {callable.value}, "callable.entry");
    }
  }
}