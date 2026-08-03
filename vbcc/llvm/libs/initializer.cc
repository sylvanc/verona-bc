#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_library_initializers()
    {
      for (const auto& library : state.libraries)
      {
        auto init_func = library / InitFunc;

        if (init_func->type() == None)
          continue;

        // InitFunc names an ordinary VIR function. Once runtime startup
        // lowering exists, emit its call after memo initialization and retain
        // any callable result for shutdown finalization.
        fail(init_func, "FFI library initializer lowering is not supported");
        return false;
      }

      return true;
    }
  }
}
