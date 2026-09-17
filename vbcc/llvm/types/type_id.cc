#include "../codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType>
    lower_type_id_type(llvm::LLVMContext&, const Node&)
    {
      // Resolves a TypeId and lowers the type it names.
      return {};
    }
  }
}
