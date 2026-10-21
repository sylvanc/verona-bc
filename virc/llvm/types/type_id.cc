#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    std::optional<LoweredType> LLVMCodegen::lower_type_id(const Node& type)
    {
      for (const auto& alias : state.typealiases)
      {
        if ((alias / TypeId)->location() == type->location())
          return lower_type(alias / Type);
      }

      fail(type, "unknown type alias '" + node_text(type) + "'");
      return {};
    }
  }
}
