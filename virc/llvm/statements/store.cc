#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_store(const Node& statement)
    {
      auto reference = locals.find_value(statement / Rhs);
      auto incoming = transfer_arg(statement / Arg);
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

      auto* outgoing_storage =
        allocate_value_storage(*lowered_result, "store.outgoing.storage");
      builder.CreateCall(
        runtime.reference_exchange,
        {*reference_storage, *incoming_storage, outgoing_storage});
      auto* result = load_value_storage(
        *lowered_result, outgoing_storage, "store.result");
      return locals.bind_value(
        statement,
        statement / LocalId,
        LoweredValue{*lowered_result, result});
    }
  }
}
