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
        (owner->value == nullptr) || (runtime.reference_from_array == nullptr))
      {
        fail(statement, "constant array reference runtime is unavailable");
        return false;
      }

      auto storage =
        allocate_value_storage(*lowered_reference, "arrayref.const.storage");
      auto* index = llvm::ConstantInt::get(
        module.getDataLayout().getIntPtrType(context),
        from_chars_sep_v<uint64_t>(statement / Rhs));
      builder.CreateCall(
        runtime.reference_from_array,
        {value_storage_pointer(storage), owner->value, index});
      return locals.bind_value(
        statement,
        statement / LocalId,
        load_value_storage(storage, "arrayref.const"));
    }
  }
}
