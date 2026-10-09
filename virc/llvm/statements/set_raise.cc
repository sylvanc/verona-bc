#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    // SetRaise replaces the current frame's raise target and returns the old
    // one.
    bool LLVMCodegen::emit_set_raise(const Node& statement)
    {
      if (runtime.frame_set_raise_target == nullptr)
      {
        fail(statement, "LLVM raise-target runtime context is unavailable");
        return false;
      }

      auto source = locals.find_value(statement / Rhs);

      if (!source)
      {
        fail(
          statement,
          "setraise of unknown local '" + node_text(statement / Rhs) + "'");
        return false;
      }

      Node type = U64;
      auto lowered = lower_type(type);

      if (!lowered)
        return false;

      if ((source->type != *lowered) || (source->value == nullptr))
      {
        fail(statement, "setraise source must have type u64");
        return false;
      }

      auto* previous = builder.CreateCall(
        runtime.frame_set_raise_target,
        {source->value},
        "raise.target.previous");
      return locals.bind_value(
        statement, statement / LocalId, LoweredValue{*lowered, previous});
    }
  }
}