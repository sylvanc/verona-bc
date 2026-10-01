#pragma once

#include "export.h"
#include "function.h"
#include "region.h"
#include "value.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  /** C-callable entry point used by the runtime to finalize object data. */
  using FinalizerThunk = void (*)(void*);

  /** Runtime representation of a field in generated object data. */
  struct Field
  {
    uintptr_t offset;
    uintptr_t size;
    uintptr_t type_id;
    ValueType value_type;
    uintptr_t id = 0;
  };

  /** Runtime representation used for a method dispatch-table entry. */
  struct Method
  {
    uintptr_t id;
    const Function* func;
  };

  /**
   * Static metadata for one generated class.
   *
   * Generated descriptors for empty classes point singleton at their
   * compiler-emitted immortal data address. Non-empty classes set it to null.
   */
  struct Class
  {
    static constexpr uintptr_t final_method_id = 0;

    uintptr_t id;
    const char* name;
    uintptr_t data_size;
    uintptr_t data_alignment;
    uintptr_t field_count;
    const Field* fields;
    uintptr_t method_count;
    const Method* methods;
    void* singleton;
    FinalizerThunk finalizer_thunk = nullptr;

    const Function* method(uintptr_t method_id) const;
    const Field* field(uintptr_t field_id) const;
    const Function* finalizer() const;
  };
}

using vrt_field = vrt::Field;
using vrt_method = vrt::Method;
using vrt_class = vrt::Class;
using vrt_finalizer_thunk = vrt::FinalizerThunk;
#else
/** C-callable entry point used by the runtime to finalize object data. */
typedef void (*vrt_finalizer_thunk)(void*);

/** Runtime representation of a field in generated object data. */
typedef struct vrt_field
{
  uintptr_t offset;
  uintptr_t size;
  uintptr_t type_id;
  uintptr_t value_type;
  uintptr_t id;
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
  uintptr_t data_size;
  uintptr_t data_alignment;
  uintptr_t field_count;
  const vrt_field* fields;
  uintptr_t method_count;
  const vrt_method* methods;
  void* singleton;
  vrt_finalizer_thunk finalizer_thunk;
} vrt_class;

#endif

#if defined(__cplusplus)
extern "C"
{
#endif

  /**
   * Allocate and initialize an object in the current frame-local region.
   *
   * packed_args points at a field-layout argument packet: every argument is
   * stored at the offset and with the representation described by the
   * corresponding field metadata. The call consumes the ownership carried
   * by managed values in the packet.
   */
  VRT_EXPORT void*
  vrt_object_new(const vrt_class* cls, uintptr_t argc, const void* packed_args);

  /** Allocate and initialize an object in current logical-frame storage. */
  VRT_EXPORT void* vrt_object_stack(
    const vrt_class* cls, uintptr_t argc, const void* packed_args);

  /**
   * Allocate in the region containing region_locator.
   *
   * region_locator is a borrowed object data address and is resolved before
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

  /** Return the immutable generated class ID for borrowed object data. */
  VRT_EXPORT uintptr_t vrt_object_class_id(const void* data_address);

  /**
   * Resolve method_id in the immutable dispatch table for object data.
   *
   * Returns null when the object's class does not provide the method. The
   * returned function metadata has static lifetime and carries no ownership.
   */
  VRT_EXPORT const vrt_func*
  vrt_object_lookup(const void* data_address, uintptr_t method_id);

  /** Add one owning root reference to an object data address. */
  VRT_EXPORT void vrt_object_retain(void* data_address);

  /** Consume one owning root reference to an object data address. */
  VRT_EXPORT void vrt_object_release(void* data_address);

  /**
   * Add one external pin reference to an object data address.
   *
   * Frame-local graphs are first relocated to a fresh RC region. Stack
   * objects cannot be pinned. Every successful pin must be balanced by one
   * call to vrt_object_unpin.
   */
  VRT_EXPORT void vrt_object_pin(void* data_address);

  /** Consume one external pin reference to an object data address. */
  VRT_EXPORT void vrt_object_unpin(void* data_address);

  /** Make the graph reachable from an object deeply immutable. */
  VRT_EXPORT void vrt_object_freeze(void* data_address);

  /**
   * Relocate a current-frame-local object so it can be returned safely.
   *
   * Objects that already outlive the current frame, including immortal
   * singletons and heap-region objects, are left unchanged.
   */
  VRT_EXPORT void vrt_object_escape(void* data_address);

  /** Reject an object that will be reclaimed by current-frame tailcall reuse. */
  VRT_EXPORT void vrt_object_validate_tailcall(void* data_address);

#if defined(__cplusplus)
}
#endif
