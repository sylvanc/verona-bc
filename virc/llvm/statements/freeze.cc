#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_freeze(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto src = statement / Rhs;
      auto source = locals.find_value(src);

      if (!source)
      {
        fail(statement, "freeze of unknown local '" + node_text(src) + "'");
        return false;
      }

      switch (source->type.runtime_type)
      {
        case vrt::ValueType::object:
          if ((runtime.object_freeze == nullptr) || (source->value == nullptr))
          {
            fail(statement, "object freeze runtime is unavailable");
            return false;
          }
          builder.CreateCall(runtime.object_freeze, {source->value});
          break;

        case vrt::ValueType::array:
          if ((runtime.array_freeze == nullptr) || (source->value == nullptr))
          {
            fail(statement, "array freeze runtime is unavailable");
            return false;
          }
          builder.CreateCall(runtime.array_freeze, {source->value});
          break;

        default:
          break;
      }

      auto value = locals.copy_value(statement, src);
      if (!value)
        return false;

      return locals.bind_value(statement, dst, *value);
    }
  }
}
