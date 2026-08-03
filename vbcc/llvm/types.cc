#include "codegen.h"

#include <llvm/IR/DerivedTypes.h>
#include <utility>

namespace vbcc
{
  namespace llvm_backend
  {
    /* maps VIR types into LLVM types and function
     * signatures. */
    std::optional<LoweredType> LLVMCodegen::lower_type(const Node& type)
    {
      if (type == None)
      {
        return LoweredType{
          ValueKind::None,
          llvm::Type::getVoidTy(context),
          nullptr,
          OwnershipKind::Trivial};
      }

      if (type == I32)
      {
        auto* llvm_type = llvm::Type::getInt32Ty(context);
        return LoweredType{
          ValueKind::I32, llvm_type, llvm_type, OwnershipKind::Trivial};
      }

      if (type == Bool)
      {
        auto* llvm_type = llvm::Type::getInt1Ty(context);
        return LoweredType{
          ValueKind::Bool, llvm_type, llvm_type, OwnershipKind::Trivial};
      }

      fail(type, "unsupported type '" + std::string(type->type().str()) + "'");
      return {};
    }
  }
}
