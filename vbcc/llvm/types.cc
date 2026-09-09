#include "codegen.h"

#include <cassert>

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<LoweredType>
    lower_builtin_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_dyn_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_class_id_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_type_id_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_union_type(llvm::LLVMContext& context, const Node& type);

    std::optional<LoweredType>
    lower_tuple_type(llvm::LLVMContext& context, const Node& type);

    // wfType dispatch
    std::optional<LoweredType> LLVMCodegen::lower_type(const Node& type)
    {
      assert(type->type().in(
        {None, Bool,  I8,    I16,     I32,    I64,   U8,       U16, U32,
         U64,  ILong, ULong, ISize,   USize,  F32,   F64,      Ptr, Array,
         Ref,  Cown,  Dyn,   ClassId, TypeId, Union, TupleType}));

      std::optional<LoweredType> lowered;

      if (type->type().in({None, Bool, I8,  I16,   I32,   I64,   U8,
                           U16,  U32,  U64, ILong, ULong, ISize, USize,
                           F32,  F64,  Ptr, Array, Ref,   Cown}))
        lowered = lower_builtin_type(context, type);
      else if (type == Dyn)
        lowered = lower_dyn_type(context, type);
      else if (type == ClassId)
        lowered = lower_class_id_type(context, type);
      else if (type == TypeId)
        lowered = lower_type_id_type(context, type);
      else if (type == Union)
        lowered = lower_union_type(context, type);
      else
      {
        assert(type == TupleType);
        lowered = lower_tuple_type(context, type);
      }

      if (lowered)
        return lowered;

      fail(
        type,
        "type lowering is not implemented for '" +
          std::string(type->type().str()) + "'");
      return {};
    }
  }
}
