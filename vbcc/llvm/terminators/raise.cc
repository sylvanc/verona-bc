#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_raise(const Node& statement)
    {
      fail(statement, "raise terminator lowering is not implemented");
      return false;
    }
  }
}
