#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_tailcall(const Node& statement)
    {
      fail(statement, "tailcall terminator lowering is not implemented");
      return false;
    }
  }
}
