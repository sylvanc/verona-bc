#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_array_ref(const Node& statement)
    {
      auto owner = transfer_arg(statement / Arg);
      auto index = locals.find_value(statement / Rhs);
      auto reference_type = resolve_local_type(statement / LocalId);
      Node usize = USize;
      auto usize_type = lower_type(usize);
      if (!owner || !index || !reference_type || !usize_type)
        return false;

      auto lowered_reference = lower_type(*reference_type);
      if (!lowered_reference)
        return false;

      if (
        (owner->type.runtime_type != vrt::ValueType::array) ||
        (owner->value == nullptr) || (index->type != *usize_type) ||
        (index->value == nullptr) || (runtime.reference_from_array == nullptr))
      {
        fail(statement, "array reference runtime is unavailable");
        return false;
      }

      auto storage =
        allocate_value_storage(*lowered_reference, "arrayref.storage");
      builder.CreateCall(
        runtime.reference_from_array,
        {value_storage_pointer(storage), owner->value, index->value});
      return locals.bind_value(
        statement,
        statement / LocalId,
        load_value_storage(storage, "arrayref"));
    }
  }
}
