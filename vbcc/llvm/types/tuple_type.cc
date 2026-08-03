#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType> lower_tuple_type(llvm::LLVMContext&, const Node&)
    {
      // Lowers TupleType elements to an LLVM tuple layout.
      return {};
    }
  }
}
