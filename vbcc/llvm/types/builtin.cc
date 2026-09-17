#include "../codegen.h"

#include <cassert>

namespace vbcc
{
  namespace llvm_backend
  {
    LoweredType
    lower_primitive_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_array_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_ref_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_cown_type(llvm::LLVMContext& context, const Node& type);

    // wfBuiltinType dispatch
    std::optional<LoweredType>
    lower_builtin_type(llvm::LLVMContext& context, const Node& type)
    {
      assert(type->type().in({None, Bool, I8,  I16,   I32,   I64,   U8,
                              U16,  U32,  U64, ILong, ULong, ISize, USize,
                              F32,  F64,  Ptr, Array, Ref,   Cown}));

      if (
        type->type().in(
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
           Ptr}))
        return lower_primitive_type(context, type);

      if (type == Array)
        return lower_array_type(context, type);

      if (type == Ref)
        return lower_ref_type(context, type);

      assert(type == Cown);
      return lower_cown_type(context, type);
    }
  }
}
