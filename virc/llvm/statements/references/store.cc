#include "../../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_store(const Node& statement)
    {
      auto reference = locals.find_value(statement / Rhs);
      auto incoming = transfer_reference_arg(statement / Arg);
      auto result_type = resolve_local_type(statement / LocalId);
      if (!reference || !incoming || !result_type)
        return false;

      auto lowered_result = lower_type(*result_type);
      if (!lowered_result)
        return false;

      if (
        (reference->type.runtime_type != vrt::ValueType::reference) ||
        (reference->value == nullptr) ||
        (runtime.reference_exchange == nullptr))
      {
        fail(statement, "reference exchange runtime is unavailable");
        return false;
      }

      auto reference_storage = materialize_value_storage(
        statement, *reference, "store.reference.storage");
      auto incoming_storage = materialize_value_storage(
        statement, *incoming, "store.incoming.storage");
      if (!reference_storage || !incoming_storage)
        return false;

      auto outgoing_storage =
        allocate_value_storage(*lowered_result, "store.outgoing.storage");
      builder.CreateCall(
        runtime.reference_exchange,
        {value_storage_pointer(*reference_storage),
         value_storage_pointer(*incoming_storage),
         value_storage_pointer(outgoing_storage)});
      return locals.bind_value(
        statement,
        statement / LocalId,
        load_value_storage(outgoing_storage, "store.result"));
    }
  }
}
