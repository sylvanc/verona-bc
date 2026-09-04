#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

  /** Runtime region implementation selected by a Region allocation. */
  typedef uint8_t vrt_region_type;

  enum
  {
    VRT_REGION_RC = 0,
    VRT_REGION_ARENA = 1
  };

#if defined(__cplusplus)
}
#endif
