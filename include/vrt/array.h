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

  /** Allocate an array in the region containing an object data address. */
  VRT_EXPORT void*
  vrt_array_heap(const void* region_locator, uintptr_t type_id, uintptr_t size);

  /** Allocate an array as the entry point of a new region. */
  VRT_EXPORT void* vrt_array_region(
    vrt_region_type region_type, uintptr_t type_id, uintptr_t size);

  /** Add one owning root reference to an array element-storage pointer. */
  VRT_EXPORT void vrt_array_retain(void* elements);

  /** Consume one owning root reference to an array element-storage pointer.
   */
  VRT_EXPORT void vrt_array_release(void* elements);

  /** Make the graph reachable from an array deeply immutable. */
  VRT_EXPORT void vrt_array_freeze(void* elements);

  /** Relocate a current-frame-local array so it can be returned safely. */
  VRT_EXPORT void vrt_array_escape(void* elements);

  /** Copy a range of encoded elements between arrays. */
  VRT_EXPORT void vrt_array_copy(
    void* destination_elements,
    uintptr_t destination_offset,
    void* source_elements,
    uintptr_t source_offset,
    uintptr_t length);

  /** Fill a range with the encoded value stored at fill_value. */
  VRT_EXPORT void vrt_array_fill(
    void* destination_elements,
    uintptr_t offset,
    uintptr_t length,
    const void* fill_value);

  /** Compare two primitive array ranges lexicographically. */
  VRT_EXPORT int64_t vrt_array_compare(
    void* left_elements,
    uintptr_t left_offset,
    void* right_elements,
    uintptr_t right_offset,
    uintptr_t length);

#if defined(__cplusplus)
}
#endif
