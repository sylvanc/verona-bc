#pragma once

#include "export.h"
#include "types.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

  /** Static metadata describing a generated Verona function. */
  typedef struct vrt_function_descriptor
  {
    uint64_t id;
    const char* name;
  } vrt_function_descriptor;

  /**
   * Push a logical frame for an immediately following Verona function call.
   *
   * function may be null. A non-null descriptor must remain alive until the
   * frame is left or rebound by a tailcall.
   *
   * A logical thread must already be bound to this native thread by libvrt.
   * Failure to allocate a frame or assign it an identity terminates the
   * process.
   */
  VRT_EXPORT vrt_frame*
  vrt_frame_enter(const vrt_function_descriptor* function);

  /**
   * Leave and destroy the current logical frame.
   *
   * The thread bound to this native thread must have a current frame.
   */
  VRT_EXPORT void vrt_frame_leave(void);

  /**
   * Reuse the current logical frame for an immediately following tailcall.
   *
   * The current frame keeps its identity, parent, region, and teardown
   * boundaries. Its function descriptor is replaced before this function
   * returns; the tailcalled function must not enter another frame.
   *
   * Lowering is responsible for moving arguments and releasing other locals
   * before calling this function. function may be null; a non-null descriptor
   * must remain alive until the frame is left or rebound again.
   */
  VRT_EXPORT void
  vrt_frame_reuse(const vrt_function_descriptor* function);

  /**
   * Return the current logical frame's raise target identity.
   *
   * The calling native thread must have a current logical frame.
   */
  VRT_EXPORT uint64_t vrt_frame_get_raise_target(void);

  /**
   * Replace the current logical frame's raise target and return the old one.
   *
   * target is stored without validation. vrt_frame_raise validates that the
   * target still names an active ancestor when a raise is performed. The
   * calling native thread must have a current logical frame and no pending
   * tailcall.
   */
  VRT_EXPORT uint64_t vrt_frame_set_raise_target(uint64_t target);

  /**
   * Return the current frame's setjmp-compatible continuation storage.
   *
   * Generated function prologues save their native continuation here. The
   * storage remains valid until the current logical frame is left.
   */
  VRT_EXPORT void* vrt_frame_raise_continuation(void);

  /**
   * Raise a type-erased value to the current frame's raise target.
   *
   * This tears down every logical frame above the target and transfers
   * control to the continuation saved by the target function. The payload is
   * recovered there with vrt_frame_take_raised_value. This function does not
   * return. An invalid or inactive target terminates the process.
   */
  VRT_EXPORT void vrt_frame_raise(uint64_t value);

  /**
   * Consume the value associated with a raise resumed in the current frame.
   *
   * Calling this without a pending raise for the current frame terminates the
   * process.
   */
  VRT_EXPORT uint64_t vrt_frame_take_raised_value(void);

  /** Return the parent frame, or null for a root frame. */
  VRT_EXPORT vrt_frame* vrt_frame_parent(vrt_frame* frame);

  /** Return the stable identity of a frame, or zero for a null frame. */
  VRT_EXPORT uint64_t vrt_frame_id(const vrt_frame* frame);

  /** Return the function currently associated with a frame. */
  VRT_EXPORT const vrt_function_descriptor*
  vrt_frame_function(const vrt_frame* frame);

#if defined(__cplusplus)
}
#endif
