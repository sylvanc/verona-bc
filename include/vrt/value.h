#pragma once

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  /** Runtime representation and ownership category of a Verona value. */
  enum class ValueType : uintptr_t
  {
    none = 0,
    scalar = 1,
    raw_pointer = 2,
    object = 3,
    array = 4,
    reference = 5,
    cown = 6,
    dynamic = 7,
    aggregate = 8,
  };
}

using vrt_value_type = vrt::ValueType;

inline constexpr auto VRT_VALUE_TYPE_NONE = vrt::ValueType::none;
inline constexpr auto VRT_VALUE_TYPE_SCALAR = vrt::ValueType::scalar;
inline constexpr auto VRT_VALUE_TYPE_RAW_POINTER = vrt::ValueType::raw_pointer;
inline constexpr auto VRT_VALUE_TYPE_OBJECT = vrt::ValueType::object;
inline constexpr auto VRT_VALUE_TYPE_ARRAY = vrt::ValueType::array;
inline constexpr auto VRT_VALUE_TYPE_REFERENCE = vrt::ValueType::reference;
inline constexpr auto VRT_VALUE_TYPE_COWN = vrt::ValueType::cown;
inline constexpr auto VRT_VALUE_TYPE_DYNAMIC = vrt::ValueType::dynamic;
inline constexpr auto VRT_VALUE_TYPE_AGGREGATE = vrt::ValueType::aggregate;
#else
/** Runtime representation and ownership category of a Verona value. */
typedef enum vrt_value_type
{
  VRT_VALUE_TYPE_NONE = 0,
  VRT_VALUE_TYPE_SCALAR = 1,
  VRT_VALUE_TYPE_RAW_POINTER = 2,
  VRT_VALUE_TYPE_OBJECT = 3,
  VRT_VALUE_TYPE_ARRAY = 4,
  VRT_VALUE_TYPE_REFERENCE = 5,
  VRT_VALUE_TYPE_COWN = 6,
  VRT_VALUE_TYPE_DYNAMIC = 7,
  VRT_VALUE_TYPE_AGGREGATE = 8
} vrt_value_type;
#endif
