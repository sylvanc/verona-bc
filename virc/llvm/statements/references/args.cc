#include "../../codegen.h"

#include <cassert>

namespace virc
{
  namespace llvm_backend
  {
    std::optional<LoweredValue>
    LLVMCodegen::transfer_reference_arg(const Node& arg)
    {
      assert(arg == Arg);
      auto kind = arg / Type;
      auto src = arg / Rhs;
      assert(kind->type().in({ArgMove, ArgCopy}));

      if (kind == ArgMove)
        return locals.move_value(arg, src);

      return locals.copy_value(arg, src);
    }
  }
}
