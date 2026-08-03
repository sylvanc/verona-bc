#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_copy(const Node& statement)
    {
      auto dst = statement / LocalId;
      auto src = statement / Rhs;
      auto* source = locals.find_value(src);

      if (!source)
      {
        fail(statement, "copy of unknown local '" + node_text(src) + "'");
        return false;
      }

      auto value = *source;

      // Copy creates another ownership obligation for managed values. Future
      // managed representations will emit their retain operation here.
      switch (value.type.ownership)
      {
        case OwnershipKind::Trivial:
          break;

        case OwnershipKind::Managed:
          fail(statement, "copy of a managed value is not supported");
          return false;
      }

      return locals.bind_value(statement, dst, value);
    }
  }
}
