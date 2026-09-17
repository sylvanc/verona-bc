#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType>
    lower_class_id_type(llvm::LLVMContext&, const Node&)
    {
      // Lowers a nominal ClassId after its LLVM layout is declared.
      return {};
    }
  }
}
