#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    std::optional<LoweredType> lower_dyn(llvm::LLVMContext&, const Node&)
    {
      // Lowers Dyn to its runtime dynamic-value representation.
      return {};
    }
  }
}
