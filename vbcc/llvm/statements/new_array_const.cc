#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_new_array_const(const Node& statement)
    {
      return emit_array_allocation(statement, runtime.array_new, {});
    }
  }
}
