#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_move(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto src = statement / Rhs;
      auto value = locals.take_value(src);

      if (!value)
      {
        fail(statement, "move of unknown local '" + node_text(src) + "'");
        return false;
      }

      // Move transfers the existing ownership obligation to the destination.
      return locals.bind_value(statement, dst, *value);
    }
  }
}
