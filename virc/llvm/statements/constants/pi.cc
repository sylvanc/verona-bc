#include "../../codegen.h"

#include <llvm/IR/Constants.h>
#include <numbers>

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_const_pi(const Node& statement)
    {
      auto lowered = lower_type(F64);

      if (!lowered)
        return false;

      auto* value = llvm::ConstantFP::get(lowered->llvm_type, std::numbers::pi);
      return locals.bind_value(
        statement, statement / LocalId, LoweredValue{*lowered, value});
    }
  }
}
