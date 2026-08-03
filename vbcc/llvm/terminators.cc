#include "codegen.h"

#include <cassert>

namespace vbcc
{
  namespace llvm_backend
  {
    // wfTerminator dispatch
    bool LLVMCodegen::emit_terminator(
      const Node& terminator, const LoweredType& return_type)
    {
      assert(terminator->type().in(
        {Tailcall, TailcallDyn, Return, Raise, Cond, Jump}));

      if (terminator == Tailcall)
        return emit_tailcall(terminator);

      if (terminator == TailcallDyn)
        return emit_tailcall_dyn(terminator);

      if (terminator == Return)
        return emit_return(terminator, return_type);

      if (terminator == Raise)
        return emit_raise(terminator);

      if (terminator == Cond)
        return emit_cond(terminator);

      assert(terminator == Jump);
      return emit_jump(terminator);
    }
  }
}
