#include "../codegen.h"

#include <cassert>
#include <llvm/IR/DerivedTypes.h>

namespace vbcc
{
  namespace llvm_backend
  {
    LoweredType lower_float_type(llvm::LLVMContext& context, const Node& type);

    LoweredType
    lower_integer_type(llvm::LLVMContext& context, const Node& type);

    namespace
    {
      LoweredType lower_none_type(llvm::LLVMContext& context)
      {
        return LoweredType{
          ValueKind::None,
          RuntimeValueKind::None,
          llvm::Type::getVoidTy(context),
          nullptr};
      }

      LoweredType lower_bool_type(llvm::LLVMContext& context)
      {
        auto* llvm_type = llvm::Type::getInt1Ty(context);
        return LoweredType{
          ValueKind::Bool, RuntimeValueKind::Scalar, llvm_type, llvm_type};
      }

      LoweredType lower_pointer_type(llvm::LLVMContext& context)
      {
        auto* llvm_type = llvm::PointerType::getUnqual(context);
        return LoweredType{
          ValueKind::Pointer,
          RuntimeValueKind::RawPointer,
          llvm_type,
          llvm_type};
      }
    }

    // wfPrimitiveType dispatch
    LoweredType
    lower_primitive_type(llvm::LLVMContext& context, const Node& type)
    {
      assert(type->type().in(
        {None,
         Bool,
         I8,
         I16,
         I32,
         I64,
         U8,
         U16,
         U32,
         U64,
         ILong,
         ULong,
         ISize,
         USize,
         F32,
         F64,
         Ptr}));

      if (type == None)
        return lower_none_type(context);

      if (type == Bool)
        return lower_bool_type(context);

      if (
        type->type().in(
          {I8, I16, I32, I64, U8, U16, U32, U64, ILong, ULong, ISize, USize}))
        return lower_integer_type(context, type);

      if (type->type().in({F32, F64}))
        return lower_float_type(context, type);

      assert(type == Ptr);
      return lower_pointer_type(context);
    }
  }
}
