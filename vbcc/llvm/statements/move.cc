#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_move(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto src = statement / Rhs;
      auto value = locals.move_value(statement, src);

      if (!value)
        return false;

      return locals.bind_value(statement, dst, *value);
    }
  }
}
