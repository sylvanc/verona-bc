#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType> lower_array_type(llvm::LLVMContext&, const Node&)
    {
      // Lowers Array(T) to the runtime array representation.
      return {};
    }
  }
}
