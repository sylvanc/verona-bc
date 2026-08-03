#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_drop(const Node& statement)
    {
      auto value_id = statement / LocalId;
      auto value = locals.take_value(value_id);

      if (!value)
      {
        fail(statement, "drop of unknown local '" + node_text(value_id) + "'");
        return false;
      }

      // Drop ends the binding's ownership obligation. Future managed
      // representations will emit their release operation here.
      switch (value->type.ownership)
      {
        case OwnershipKind::Trivial:
          break;

        case OwnershipKind::Managed:
          fail(statement, "drop of a managed value is not supported");
          return false;
      }

      return true;
    }
  }
}
