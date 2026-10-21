#include "../../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_array_ref(const Node& statement)
    {
      auto owner = transfer_reference_arg(statement / Arg);
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
        (index->value == nullptr) ||
        (lowered_reference->runtime_type != vrt::ValueType::reference) ||
        (runtime.reference_from_array == nullptr))
      {
        fail(statement, "array reference runtime is unavailable");
        return false;
      }

      return emit_reference_construction(
        statement,
        *lowered_reference,
        runtime.reference_from_array,
        {owner->value, index->value},
        "arrayref");
    }
  }
}
