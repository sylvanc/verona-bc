#include "../codegen.h"

#include <cassert>

namespace virc
{
  namespace llvm_backend
  {
    LoweredType
    lower_primitive_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_array(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_ref(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_cown(llvm::LLVMContext& context, const Node& type);

    // wfBuiltinType dispatch
    std::optional<LoweredType>
    lower_builtin_type(llvm::LLVMContext& context, const Node& type)
    {
      assert(type->type().in({None, Bool, I8,  I16,   I32,   I64,   U8,
                              U16,  U32,  U64, ILong, ULong, ISize, USize,
                              F32,  F64,  Ptr, Array, Ref,   Cown}));

      if (type->type().in(
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
        return lower_array(context, type);

      if (type == Ref)
        return lower_ref(context, type);

      assert(type == Cown);
      return lower_cown(context, type);
    }
  }
}
