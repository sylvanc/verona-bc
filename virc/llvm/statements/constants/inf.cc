#include "../../codegen.h"

#include <limits>
#include <llvm/IR/Constants.h>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_const_inf(const Node& statement)
    {
      auto lowered = lower_type(F64);

      if (!lowered)
        return false;

      auto* value = llvm::ConstantFP::get(
        lowered->llvm_type, std::numeric_limits<double>::infinity());
      return locals.bind_value(
        statement, statement / LocalId, LoweredValue{*lowered, value});
    }
  }
}
