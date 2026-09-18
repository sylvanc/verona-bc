#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_new(const Node& statement)
    {
      return emit_object_allocation(statement, runtime.object_new, {});
    }
  }
}
