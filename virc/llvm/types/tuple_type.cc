#include "../codegen.h"

namespace virc
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
