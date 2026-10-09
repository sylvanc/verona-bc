#include "../codegen.h"

#include <llvm/IR/Constants.h>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_raise(const Node& statement)
    {
      auto value_id = statement / LocalId;
      auto value = locals.move_value(statement, value_id);

      if (!value)
        return false;

      auto raise_type = lower_type(statement / Type);
      if (!raise_type)
        return false;

      if (value->type != *raise_type)
      {
        fail(statement, "raise representation mismatch");
        return false;
      }

      if (runtime.frame_raise == nullptr)
      {
        fail(statement, "LLVM raise runtime context is unavailable");
        return false;
      }

      auto storage =
        materialize_value_storage(statement, *value, "raise.value.storage");
      if (!storage)
        return false;

      auto type_id = runtime_type_id(statement / Type);
      if (!type_id)
        return false;

      auto* type_id_value = llvm::ConstantInt::get(
        module.getDataLayout().getIntPtrType(context), *type_id);
      builder.CreateCall(
        runtime.frame_raise, {type_id_value, value_storage_pointer(*storage)});
      builder.CreateUnreachable();
      return true;
    }
  }
}
