#include "../codegen.h"

#include <cassert>
#include <llvm/IR/DerivedTypes.h>

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType>
    lower_array_type(llvm::LLVMContext& context, const Node& type)
    {
      assert(type == Array);
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      return LoweredType{
        IRValueType::Pointer,
        vrt::ValueType::array,
        pointer_type,
        pointer_type};
    }
  }
}
