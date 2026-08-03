#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_jump(const Node& statement)
    {
      auto* target = blocks.find(statement, statement / LabelId);

      if (target == nullptr)
        return false;

      builder.CreateBr(target);
      return true;
    }
  }
}
