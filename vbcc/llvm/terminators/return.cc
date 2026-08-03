#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_return(
      const Node& statement, const LoweredType& return_type)
    {
      auto value_id = statement / LocalId;
      auto* value = locals.find_value(value_id);

      if (!value)
      {
        fail(
          statement, "return of unknown local '" + node_text(value_id) + "'");
        return false;
      }

      if (value->type != return_type)
      {
        fail(statement, "return representation mismatch");
        return false;
      }

      if (return_type.kind == ValueKind::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(value->value);

      return true;
    }
  }
}
