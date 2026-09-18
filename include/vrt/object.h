#pragma once

#include "export.h"
#include "function.h"
#include "region.h"
#include "value.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  /** Runtime representation of a field in a generated class payload. */
  struct Field
  {
    uintptr_t offset;
    uintptr_t size;
    uintptr_t type_id;
    ValueType value_type;
  };

  /** Runtime representation used for a method dispatch-table entry. */
  struct Method
  {
    uintptr_t id;
    const Func* func;
  };

  /**
   * Static metadata for one generated class.
   *
   * Generated descriptors for empty classes point singleton at their
   * compiler-emitted immortal payload. Non-empty classes set it to null.
   */
  struct Class
  {
    uintptr_t id;
    const char* name;
    uintptr_t payload_size;
    uintptr_t payload_alignment;
    uintptr_t field_count;
    const Field* fields;
    uintptr_t method_count;
    const Method* methods;
    void* singleton;
  };
}

using vrt_field = vrt::Field;
using vrt_method = vrt::Method;
using vrt_class = vrt::Class;
#else
/** Runtime representation of a field in a generated class payload. */
typedef struct vrt_field
{
  uintptr_t offset;
  uintptr_t size;
  uintptr_t type_id;
  uintptr_t value_type;
} vrt_field;

/** Runtime representation used for a method dispatch-table entry. */
typedef struct vrt_method
{
  uintptr_t id;
  const vrt_func* func;
} vrt_method;

/** Static metadata for one generated class. */
typedef struct vrt_class
{
  uintptr_t id;
  const char* name;
  uintptr_t payload_size;
  uintptr_t payload_alignment;
  uintptr_t field_count;
  const vrt_field* fields;
  uintptr_t method_count;
  const vrt_method* methods;
  void* singleton;
} vrt_class;

#endif

#if defined(__cplusplus)
extern "C"
{
#endif

  /**
   * Allocate and initialize an object in the current frame-local region.
   *
   * packed_args points at a payload-shaped argument packet: every argument is
   * stored at the offset and with the representation described by the
   * corresponding field metadata. The call consumes the ownership carried
   * by managed values in the packet.
   */
  VRT_EXPORT void*
  vrt_object_new(const vrt_class* cls, uintptr_t argc, const void* packed_args);

  /**
   * Allocate in the region containing region_locator.
   *
   * region_locator is a borrowed object payload pointer and is resolved before
   * singleton handling. packed_args has the same form and ownership contract
   * as for vrt_object_new.
   */
  VRT_EXPORT void* vrt_object_heap(
    const void* region_locator,
    const vrt_class* cls,
    uintptr_t argc,
    const void* packed_args);

  /**
   * Create a region and allocate its entry-point object.
   *
   * Empty classes cannot be region entry points. packed_args has the same
   * form and ownership contract as for vrt_object_new.
   */
  VRT_EXPORT void* vrt_object_region(
    vrt_region_type region_type,
    const vrt_class* cls,
    uintptr_t argc,
    const void* packed_args);

  /** Add one owning register reference to an object payload. */
  VRT_EXPORT void vrt_object_retain(void* payload);

  /** Consume one owning register reference to an object payload. */
  VRT_EXPORT void vrt_object_release(void* payload);

  /**
   * Relocate a current-frame-local object so it can be returned safely.
   *
   * Objects that already outlive the current frame, including immortal
   * singletons and heap-region objects, are left unchanged.
   */
  VRT_EXPORT void vrt_object_escape(void* payload);

#if defined(__cplusplus)
}
#endif
