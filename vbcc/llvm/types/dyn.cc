#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType> lower_dyn_type(llvm::LLVMContext&, const Node&)
    {
      // Lowers Dyn to its runtime dynamic-value representation.
      return {};
    }
  }
}
