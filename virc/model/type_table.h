#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace virc
{
  enum class PrimitiveKind : uint8_t
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
  };

  inline constexpr size_t operator+(PrimitiveKind value)
  {
    return static_cast<size_t>(value);
  }

  inline constexpr auto PrimitiveTypeCount = +PrimitiveKind::Ptr + 1;

  enum class TypeKind
  {
    Array,
    Cown,
    Ref,
    Union,
    Tuple,
  };

  struct TypeInfo
  {
    TypeKind kind;
    std::vector<size_t> elements;

    bool operator==(const TypeInfo&) const = default;
  };

  struct TypeInfoHash
  {
    size_t operator()(const TypeInfo& type) const noexcept;
  };
}