#pragma once

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  /** Runtime region implementation selected by a Region allocation. */
  enum class RegionType : uint8_t
  {
    rc = 0,
    arena = 1,
  };
}

using vrt_region_type = vrt::RegionType;

inline constexpr auto VRT_REGION_RC = vrt::RegionType::rc;
inline constexpr auto VRT_REGION_ARENA = vrt::RegionType::arena;
#else
/** Runtime region implementation selected by a Region allocation. */
typedef uint8_t vrt_region_type;

enum
{
  VRT_REGION_RC = 0,
  VRT_REGION_ARENA = 1
};
#endif
