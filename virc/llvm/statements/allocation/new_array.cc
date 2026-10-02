#include "../../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_new_array(const Node& statement)
    {
      return emit_array_allocation(statement, runtime.array_new, {});
    }
  }
}
