#include "../../codegen.h"

#include <llvm/IR/Constants.h>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_field_ref(const Node& statement)
    {
      auto owner = transfer_reference_arg(statement / Arg);
      if (!owner)
        return false;

      auto reference_type = resolve_local_type(statement / LocalId);
      auto field_id = state.get_field_id(statement / FieldId);
      if (!reference_type || !field_id)
      {
        if (!field_id)
          fail(statement / FieldId, "field reference has no assigned field ID");
        return false;
      }

      auto lowered_reference = lower_type(*reference_type);
      if (!lowered_reference)
        return false;

      if (
        (owner->type.runtime_type != vrt::ValueType::object) ||
        (owner->value == nullptr) ||
        (lowered_reference->runtime_type != vrt::ValueType::reference) ||
        (runtime.reference_from_field == nullptr))
      {
        fail(statement, "field reference runtime is unavailable");
        return false;
      }

      auto* id = llvm::ConstantInt::get(
        module.getDataLayout().getIntPtrType(context), *field_id);
      return emit_reference_construction(
        statement,
        *lowered_reference,
        runtime.reference_from_field,
        {owner->value, id},
        "fieldref");
    }
  }
}
