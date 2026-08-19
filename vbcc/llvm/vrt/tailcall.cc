#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_prepare_tailcall(
      const Node& statement, llvm::Value* function_descriptor)
    {
      if (
        (runtime.frame_prepare_tailcall == nullptr) ||
        (function_descriptor == nullptr))
      {
        fail(statement, "LLVM tailcall runtime context is unavailable");
        return false;
      }

      builder.CreateCall(runtime.frame_prepare_tailcall, {function_descriptor});
      return true;
    }
  }
}
