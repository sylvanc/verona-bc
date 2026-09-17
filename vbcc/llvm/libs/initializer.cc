#include "../codegen.h"

#include <cassert>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_library_initializers()
    {
      assert(libraries.size() == state.libraries.size());

      for (size_t i = 0; i < libraries.size(); ++i)
      {
        auto& library = libraries.at(i);

        if (!library.init_function_id)
          continue;

        // InitFunc names an ordinary VIR function. Once runtime startup
        // lowering exists, emit its call after memo initialization and retain
        // any callable result for shutdown finalization.
        auto init_func = state.libraries.at(i) / InitFunc;
        fail(init_func, "FFI library initializer lowering is not supported");
        return false;
      }

      return true;
    }
  }
}
