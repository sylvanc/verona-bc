#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType> lower_union_type(llvm::LLVMContext&, const Node&)
    {
      // Lowers Union members to their shared runtime representation.
      return {};
    }
  }
}
