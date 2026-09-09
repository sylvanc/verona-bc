#include "../codegen.h"

#include <cassert>
#include <llvm/IR/DerivedTypes.h>

namespace vbcc
{
  namespace llvm_backend
  {
    // wfFloatType family
    LoweredType lower_float_type(llvm::LLVMContext& context, const Node& type)
    {
      assert(type->type().in({F32, F64}));

      auto* llvm_type = type == F32 ? llvm::Type::getFloatTy(context) :
                                      llvm::Type::getDoubleTy(context);
      return LoweredType{
        ValueKind::Float, RuntimeValueKind::Scalar, llvm_type, llvm_type};
    }
  }
}
