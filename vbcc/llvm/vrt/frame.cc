#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_leave_frame(const Node& statement)
    {
      if (runtime.frame_leave == nullptr)
      {
        fail(statement, "LLVM frame runtime is unavailable");
        return false;
      }

      builder.CreateCall(runtime.frame_leave);
      return true;
    }
  }
}
