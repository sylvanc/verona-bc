#pragma once

#include "export.h"
#include "value.h"

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

#if defined(__cplusplus)
extern "C"
{
#endif

  /**
   * Merge two mutable object or array values into one RC region.
   *
   * Values already in one region, two frame-local values, or values without a
   * mutable region are no-ops. Unsupported ownership combinations raise
   * VRT_ERROR_BAD_MERGE without changing either graph.
   */
  VRT_EXPORT void vrt_region_merge(
    vrt_value_type left_type,
    void* left_data,
    vrt_value_type right_type,
    void* right_data);

#if defined(__cplusplus)
}
#endif
