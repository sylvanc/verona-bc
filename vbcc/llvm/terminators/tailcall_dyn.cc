#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_tailcall_dyn(const Node& statement)
    {
      fail(
        statement, "dynamic tailcall terminator lowering is not implemented");
      return false;
    }
  }
}
