#pragma once

#include "ident.h"

#include <vbci.h>
#include <vector>

namespace vbci
{
  enum class TypeTag
  {
    Base,
    Array,
    Cown,
    Union,
  };

  inline constexpr uint32_t operator+(TypeTag tag)
  {
    return static_cast<uint32_t>(tag);
  }

  // Space for 15 levels of ref on 33 million classes and 33 million complex
  // types.
  struct TypeId
  {
    static constexpr auto MaxRef = (1 << 4) - 1;
    static constexpr auto Argv = 0;

    uint32_t tag : 2;
    uint32_t ref : 4;
    uint32_t idx : 26;

    TypeId();

    static TypeId val(ValueType type);
    static TypeId dyn();
    static TypeId cls(size_t idx);
    static TypeId leb(size_t leb);
    static TypeId argv();

    bool is_dyn() const;
    bool is_val() const;
    bool is_class() const;
    bool is_complex() const;
    bool is_base() const;
    bool is_ref() const;
    bool is_array() const;
    bool is_cown() const;
    bool is_union() const;

    ValueType val() const;
    size_t cls() const;
    size_t complex() const;

    TypeId array() const;
    TypeId unarray() const;
    TypeId cown() const;
    TypeId uncown() const;
    TypeId make_ref() const;

    bool operator==(const TypeId& that) const;
    bool operator<(const TypeId& that) const;
  };

  struct ComplexType
  {
    // In complex types, to make subtype checking easier, we don't mix ref,
    // array, or cown with the underlying type.
    enum Tag
    {
      Base,
      Ref,
      Array,
      Cown,
      Union,
    };

    Tag tag;

    union
    {
      uint32_t depth;
      uint32_t type_id;
    };

    std::vector<ComplexType> children;

    ComplexType() : tag(Base), depth(0) {}
  };
}
