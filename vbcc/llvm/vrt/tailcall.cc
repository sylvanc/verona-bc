#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_reuse_frame(
      const Node& statement, llvm::Value* function_descriptor)
    {
      if (
        (runtime.frame_reuse == nullptr) ||
        (function_descriptor == nullptr))
      {
        fail(statement, "LLVM tailcall runtime context is unavailable");
        return false;
      }

      builder.CreateCall(runtime.frame_reuse, {function_descriptor});
      return true;
    }
  }
}
