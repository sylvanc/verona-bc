#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_cond(const Node& statement)
    {
      auto condition_id = statement / LocalId;
      auto* condition = locals.find_value(condition_id);

      if (!condition)
      {
        fail(
          statement,
          "condition uses unknown local '" + node_text(condition_id) + "'");
        return false;
      }

      if (
        (condition->type.kind != ValueKind::Bool) ||
        (condition->value == nullptr))
      {
        fail(statement, "condition representation is not bool");
        return false;
      }

      auto* true_block = blocks.find(statement, statement / Lhs);
      auto* false_block = blocks.find(statement, statement / Rhs);

      if ((true_block == nullptr) || (false_block == nullptr))
        return false;

      builder.CreateCondBr(condition->value, true_block, false_block);
      return true;
    }
  }
}
