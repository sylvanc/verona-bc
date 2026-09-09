#include "../codegen.h"

#include <cassert>
#include <llvm/IR/DerivedTypes.h>

namespace vbcc
{
  namespace llvm_backend
  {
    // wfIntType family
    LoweredType lower_integer_type(llvm::LLVMContext& context, const Node& type)
    {
      assert(type->type().in(
        {I8, I16, I32, I64, U8, U16, U32, U64, ILong, ULong, ISize, USize}));

      unsigned width = 0;

      if (type->type().in({I8, U8}))
        width = 8;
      else if (type->type().in({I16, U16}))
        width = 16;
      else if (type->type().in({I32, U32}))
        width = 32;
      else
      {
        assert(type->type().in({I64, U64, ILong, ULong, ISize, USize}));
        // VIR and the bytecode backend currently use a 64-bit representation
        // for long and size integers. configure_target() must eventually make
        // these target-dependent for cross-target LLVM emission.
        width = 64;
      }

      auto kind = type->type().in({I8, I16, I32, I64, ILong, ISize}) ?
        ValueKind::SignedInteger :
        ValueKind::UnsignedInteger;
      auto* llvm_type = llvm::IntegerType::get(context, width);
      return LoweredType{kind, RuntimeValueKind::Scalar, llvm_type, llvm_type};
    }
  }
}
