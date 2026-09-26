#pragma once

#include <vbc/format.h>

namespace vbci
{
  using namespace vbc;

  // The primitive values intentionally match vbc::PrimitiveType. The remaining
  // values classify interpreter layouts or tag live VBCI values and are never
  // encoded as primitive IDs in VBC.
  enum class ValueType : uint8_t
  {
    None,
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
    Ptr,
    Object,
    Array,
    Cown,
    RegisterRef,
    FieldRef,
    ArrayRef,
    CownRef,
    Function,
    Error,
    Dyn,
    Invalid,
  };

  inline constexpr size_t operator+(ValueType value)
  {
    return static_cast<size_t>(value);
  }

  inline constexpr ValueType value_type(PrimitiveType primitive)
  {
    return static_cast<ValueType>(+primitive);
  }

  static_assert(+ValueType::None == +PrimitiveType::None);
  static_assert(+ValueType::Bool == +PrimitiveType::Bool);
  static_assert(+ValueType::I8 == +PrimitiveType::I8);
  static_assert(+ValueType::I16 == +PrimitiveType::I16);
  static_assert(+ValueType::I32 == +PrimitiveType::I32);
  static_assert(+ValueType::I64 == +PrimitiveType::I64);
  static_assert(+ValueType::U8 == +PrimitiveType::U8);
  static_assert(+ValueType::U16 == +PrimitiveType::U16);
  static_assert(+ValueType::U32 == +PrimitiveType::U32);
  static_assert(+ValueType::U64 == +PrimitiveType::U64);
  static_assert(+ValueType::ILong == +PrimitiveType::ILong);
  static_assert(+ValueType::ULong == +PrimitiveType::ULong);
  static_assert(+ValueType::ISize == +PrimitiveType::ISize);
  static_assert(+ValueType::USize == +PrimitiveType::USize);
  static_assert(+ValueType::F32 == +PrimitiveType::F32);
  static_assert(+ValueType::F64 == +PrimitiveType::F64);
  static_assert(+ValueType::Ptr == +PrimitiveType::Ptr);
}