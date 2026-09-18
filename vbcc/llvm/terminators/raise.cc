#include "../codegen.h"

#include <llvm/IR/Constants.h>

namespace vbcc
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

      auto bits = pack_raised_value(statement, *value);
      if (!bits)
        return false;

      auto* value_type = llvm::ConstantInt::get(
        module.getDataLayout().getIntPtrType(context),
        static_cast<std::size_t>(value->type.runtime_type));
      builder.CreateCall(runtime.frame_raise, {value_type, *bits});
      builder.CreateUnreachable();
      return true;
    }
  }
}
