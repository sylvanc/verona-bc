#include "../../codegen.h"

#include <cassert>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_release_array_bulk_args(
      const Node& args, const std::vector<LoweredValue>& values)
    {
      assert(args->type() == Args);
      assert(args->size() == values.size());

      for (std::size_t index = 0; index < values.size(); ++index)
      {
        if (!emit_release(args->at(index), values.at(index)))
          return false;
      }

      return true;
    }
  }
}
