#pragma once

#include "location.h"

#include <vrt/value.h>

namespace vrt
{
  struct Header;
  struct Region;

  inline constexpr bool is_valid_value_type(ValueType value_type)
  {
    switch (value_type)
    {
      case ValueType::none:
      case ValueType::scalar:
      case ValueType::raw_pointer:
      case ValueType::object:
      case ValueType::array:
      case ValueType::reference:
      case ValueType::cown:
      case ValueType::dynamic:
      case ValueType::aggregate:
        return true;

      default:
        return false;
    }
  }

  inline constexpr bool is_supported_storage_type(ValueType value_type)
  {
    switch (value_type)
    {
      case ValueType::none:
      case ValueType::scalar:
      case ValueType::raw_pointer:
      case ValueType::object:
      case ValueType::array:
        return true;

      default:
        return false;
    }
  }

  inline constexpr bool is_header_type(ValueType value_type)
  {
    return (value_type == ValueType::object) ||
      (value_type == ValueType::array);
  }

  inline constexpr bool is_unmanaged_value_type(ValueType value_type)
  {
    return (value_type == ValueType::none) ||
      (value_type == ValueType::scalar) ||
      (value_type == ValueType::raw_pointer);
  }

  /** Non-owning, type-erased view over one runtime value. */
  class Value final
  {
  private:
    ValueType value_type;
    const void* data_address;

  public:
    Value(ValueType value_type, const void* data_address)
    : value_type(value_type), data_address(data_address)
    {}

    ValueType type() const
    {
      return value_type;
    }

    Header* header() const;
    Location location() const;
    Region* region() const;
  };
}
