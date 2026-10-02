#pragma once

#include "export.h"
#include "frame.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  enum class ReferenceKind : uintptr_t
  {
    register_slot = 0,
    object_field = 1,
    array_element = 2,
  };

  struct Reference
  {
    uintptr_t kind_flags;
    void* owner;
    void* target;
    uintptr_t content_type_id;
    uintptr_t storage_epoch;
  };
}

using vrt_reference_kind = vrt::ReferenceKind;
using vrt_reference = vrt::Reference;

inline constexpr auto VRT_REFERENCE_REGISTER =
  vrt::ReferenceKind::register_slot;
inline constexpr auto VRT_REFERENCE_FIELD =
  vrt::ReferenceKind::object_field;
inline constexpr auto VRT_REFERENCE_ARRAY =
  vrt::ReferenceKind::array_element;
#else
typedef enum vrt_reference_kind
{
  VRT_REFERENCE_REGISTER = 0,
  VRT_REFERENCE_FIELD = 1,
  VRT_REFERENCE_ARRAY = 2
} vrt_reference_kind;

typedef struct vrt_reference
{
  uintptr_t kind_flags;
  void* owner;
  void* target;
  uintptr_t content_type_id;
  uintptr_t storage_epoch;
} vrt_reference;
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

  /**
   * Create a frame-scoped reference to mutable generated-code storage.
   *
   * frame and target are borrowed. The reference remains valid while frame is
   * active and its native storage epoch is unchanged.
   */
  VRT_EXPORT void vrt_reference_from_register(
    vrt_reference* out,
    vrt_frame* frame,
    void* target,
    uintptr_t content_type_id);

  /**
   * Create a reference to a field of an owned object value.
   *
   * This consumes the caller's root reference to owned_object.
   */
  VRT_EXPORT void vrt_reference_from_field(
    vrt_reference* out, void* owned_object, uintptr_t field_id);

  /**
   * Create a reference to an element of an owned array value.
   *
   * This consumes the caller's root reference to owned_array.
   */
  VRT_EXPORT void vrt_reference_from_array(
    vrt_reference* out, void* owned_array, uintptr_t index);

  /** Add one owning reference to reference's field or array owner. */
  VRT_EXPORT void vrt_reference_retain(const vrt_reference* reference);

  /** Consume one owning reference to reference's field or array owner. */
  VRT_EXPORT void vrt_reference_release(const vrt_reference* reference);

  /** Relocate the reference owner as required for a normal return. */
  VRT_EXPORT void vrt_reference_escape(const vrt_reference* reference);

  /** Reject register references whose native storage dies at a tailcall. */
  VRT_EXPORT void
  vrt_reference_validate_tailcall(const vrt_reference* reference);

  /**
   * Copy the referenced value into output_storage and create one root
   * ownership obligation for managed results.
   */
  VRT_EXPORT void
  vrt_reference_load(const vrt_reference* reference, void* output_storage);

  /**
   * Replace the referenced value with one owned value and return the previous
   * value with one root ownership obligation.
   */
  VRT_EXPORT void vrt_reference_exchange(
    const vrt_reference* reference,
    const void* owned_incoming_storage,
    void* outgoing_storage);

#if defined(__cplusplus)
}
#endif
