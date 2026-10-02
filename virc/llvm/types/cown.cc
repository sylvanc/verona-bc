#include "../codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    std::optional<LoweredType> lower_cown(llvm::LLVMContext&, const Node&)
    {
      // Lowers Cown(T) to the runtime cown representation.
      return {};
    }
  }
}
