#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    // GetRaise reads the current frame's opaque raise target.
    bool LLVMCodegen::emit_get_raise(const Node& statement)
    {
      if (runtime.frame_get_raise_target == nullptr)
      {
        fail(statement, "LLVM raise-target runtime context is unavailable");
        return false;
      }

      Node type = U64;
      auto lowered = lower_type(type);

      if (!lowered)
        return false;

      auto* value =
        builder.CreateCall(runtime.frame_get_raise_target, {}, "raise.target");
      return locals.bind_value(
        statement, statement / LocalId, LoweredValue{*lowered, value});
    }
  }
}