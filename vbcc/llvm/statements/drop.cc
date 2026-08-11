#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_drop(const Node& statement)
    {
      return locals.drop_value(statement, statement / LocalId);
    }
  }
}
