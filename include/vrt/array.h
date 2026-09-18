#pragma once

#include "export.h"
#include "region.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

  /** Allocate a zero-initialized array in the current frame-local region. */
  VRT_EXPORT void* vrt_array_new(uintptr_t type_id, uintptr_t size);

  /** Allocate an array in the region containing an object payload. */
  VRT_EXPORT void* vrt_array_heap(
    const void* region_locator, uintptr_t type_id, uintptr_t size);

  /** Allocate an array as the entry point of a new region. */
  VRT_EXPORT void* vrt_array_region(
    vrt_region_type region_type, uintptr_t type_id, uintptr_t size);

  /** Add one owning register reference to an array element-storage pointer. */
  VRT_EXPORT void vrt_array_retain(void* payload);

  /** Consume one owning register reference to an array element-storage pointer. */
  VRT_EXPORT void vrt_array_release(void* payload);

  /** Relocate a current-frame-local array so it can be returned safely. */
  VRT_EXPORT void vrt_array_escape(void* payload);

#if defined(__cplusplus)
}
#endif
