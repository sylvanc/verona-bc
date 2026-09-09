#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_copy(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto value = locals.copy_value(statement, statement / Rhs);

      if (!value)
        return false;

      return locals.bind_value(statement, dst, *value);
    }
  }
}
