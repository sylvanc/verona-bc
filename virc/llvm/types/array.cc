#include "../codegen.h"

#include <cassert>
#include <llvm/IR/DerivedTypes.h>

namespace virc
{
  namespace llvm_backend
  {
    std::optional<LoweredType>
    lower_array(llvm::LLVMContext& context, const Node& type)
    {
      assert(type == Array);
      (void)type;
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      return LoweredType{
        IRValueType::Pointer,
        vrt::ValueType::array,
        pointer_type,
        pointer_type};
    }
  }
}
