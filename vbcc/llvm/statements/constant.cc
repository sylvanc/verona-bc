#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredValue> lower_literal(
      const Node& type,
      const Node& literal,
      const LoweredType& lowered,
      std::string& error);

    bool LLVMCodegen::emit_const(const Node& statement)
    {
      auto type = statement / Type;
      auto lowered = lower_type(type);

      if (!lowered)
        return false;

      std::string error;
      auto value = lower_literal(type, statement / Rhs, *lowered, error);

      if (!value)
      {
        fail(statement, error);
        return false;
      }

      return locals.bind_value(statement, statement / LocalId, *value);
    }
  }
}
