#include "../../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_load(const Node& statement)
    {
      auto reference = locals.find_value(statement / Rhs);
      auto result_type = resolve_local_type(statement / LocalId);
      if (!reference || !result_type)
        return false;

      auto lowered_result = lower_type(*result_type);
      if (!lowered_result)
        return false;

      if (
        (reference->type.runtime_type != vrt::ValueType::reference) ||
        (reference->value == nullptr) || (runtime.reference_load == nullptr))
      {
        fail(statement, "reference load runtime is unavailable");
        return false;
      }

      auto reference_storage = materialize_value_storage(
        statement, *reference, "load.reference.storage");
      if (!reference_storage)
        return false;

      auto result_storage =
        allocate_value_storage(*lowered_result, "load.result.storage");
      builder.CreateCall(
        runtime.reference_load,
        {value_storage_pointer(*reference_storage),
         value_storage_pointer(result_storage)});
      return locals.bind_value(
        statement,
        statement / LocalId,
        load_value_storage(result_storage, "load.result"));
    }
  }
}
