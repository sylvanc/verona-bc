#include "../codegen.h"

#include <llvm/IR/DerivedTypes.h>

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType>
    LLVMCodegen::lower_class_id_type(const Node& type)
    {
      auto name = node_text(type);
      if (classes.find(name) == classes.end())
      {
        fail(type, "unknown class '" + name + "'");
        return {};
      }

      auto* pointer_type = llvm::PointerType::getUnqual(context);
      return LoweredType{
        IRValueType::Pointer,
        vrt::ValueType::object,
        pointer_type,
        pointer_type};
    }
  }
}
