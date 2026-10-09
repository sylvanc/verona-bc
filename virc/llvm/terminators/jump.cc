#include "../codegen.h"

namespace virc
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
