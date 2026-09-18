#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_return(
      const Node& statement, const LoweredType& return_type)
    {
      auto value_id = statement / LocalId;
      auto value = locals.find_value(value_id);

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

      if (return_type.runtime_type == vrt::ValueType::object)
      {
        if ((runtime.object_escape == nullptr) || (value->value == nullptr))
        {
          fail(statement, "object escape runtime is unavailable");
          return false;
        }

        builder.CreateCall(runtime.object_escape, {value->value});
      }

      if (!emit_leave_frame(statement))
        return false;

      if (return_type.ir_type == IRValueType::None)
        builder.CreateRetVoid();
      else
        builder.CreateRet(value->value);

      return true;
    }
  }
}
