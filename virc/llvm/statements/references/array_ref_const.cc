#include "../../codegen.h"

#include <llvm/IR/Constants.h>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_array_ref_const(const Node& statement)
    {
      auto owner = transfer_reference_arg(statement / Arg);
      auto reference_type = resolve_local_type(statement / LocalId);
      if (!owner || !reference_type)
        return false;

      auto lowered_reference = lower_type(*reference_type);
      if (!lowered_reference)
        return false;

      if (
        (owner->type.runtime_type != vrt::ValueType::array) ||
        (owner->value == nullptr) ||
        (lowered_reference->runtime_type != vrt::ValueType::reference) ||
        (runtime.reference_from_array == nullptr))
      {
        fail(statement, "constant array reference runtime is unavailable");
        return false;
      }

      auto* index = llvm::ConstantInt::get(
        module.getDataLayout().getIntPtrType(context),
        from_chars_sep_v<uint64_t>(statement / Rhs));
      return emit_reference_construction(
        statement,
        *lowered_reference,
        runtime.reference_from_array,
        {owner->value, index},
        "arrayref.const");
    }
  }
}
