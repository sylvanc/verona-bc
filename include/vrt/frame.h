#pragma once

#include "export.h"
#include "function.h"

#include <stdint.h>

#if defined(__cplusplus)
namespace vrt
{
  struct Frame;
}

/** Logical Verona function frame. */
using vrt_frame = vrt::Frame;

extern "C"
{
#else
/** Logical Verona function frame. */
typedef struct vrt_frame vrt_frame;
#endif

  /**
   * Push a logical frame for an immediately following Verona function call.
   *
   * func may be null. Non-null function metadata must remain alive until the
   * frame is left or rebound by a tailcall. If an error reports func through
   * vrt_error_info, it must also remain alive while that record is inspected.
   *
   * A logical thread must already be bound to this native thread by libvrt.
  * Failure to allocate a frame or assign its stack Location terminates the
   * process.
   */
  VRT_EXPORT vrt_frame*
  vrt_frame_enter(const vrt_func* func);

  /**
   * Leave and destroy the current logical frame.
   *
   * The thread bound to this native thread must have a current frame.
   */
  VRT_EXPORT void vrt_frame_leave(void);

  /**
   * Reuse the current logical frame for an immediately following tailcall.
   *
  * The current frame keeps its stack Location, parent, region, and teardown
   * boundaries. Its function metadata is replaced before this function
   * returns; the tailcalled function must not enter another frame.
   *
   * Lowering is responsible for moving arguments and releasing other locals
   * before calling this function. func may be null; non-null function
   * metadata must remain alive until the frame is left or rebound again.
   */
  VRT_EXPORT void
  vrt_frame_reuse(const vrt_func* func);

  /**
  * Return the raw Location encoding of the current frame's raise target.
   *
   * The calling native thread must have a current logical frame.
   */
  VRT_EXPORT uint64_t vrt_frame_get_raise_target(void);

  /**
  * Replace the current logical frame's raw Location raise target and return
  * the old encoding.
   *
  * target is stored without validation. vrt_frame_raise validates that it is
  * a stack Location naming an active ancestor when a raise is performed. The
   * calling native thread must have a current logical frame and no pending
   * tailcall.
   */
  VRT_EXPORT uint64_t vrt_frame_set_raise_target(uint64_t target);

  /**
   * Return the setjmp-compatible native continuation storage associated with
   * the current logical frame.
   *
   * Generated function prologues save their native continuation in sidecar
   * storage associated with the current native-thread context. It remains
   * valid until the current logical frame is left.
   */
  VRT_EXPORT void* vrt_frame_raise_continuation(void);

  /**
   * Raise a type-erased value to the current frame's raise target.
   *
   * This tears down every logical frame above the target and transfers
   * control to the continuation saved by the target function. The payload is
   * recovered there with vrt_frame_take_raised_value. This function does not
   * return. An invalid or inactive target raises
   * VRT_ERROR_BAD_RAISE_TARGET to the active invocation catch point.
   */
  VRT_EXPORT void vrt_frame_raise(uint64_t value);

  /**
   * Consume the value associated with a raise resumed in the current frame.
   *
  * Calling this unless the current frame has just been resumed by a raise, or
  * consuming the same payload twice, terminates the process.
   */
  VRT_EXPORT uint64_t vrt_frame_take_raised_value(void);

  /** Return the parent frame, or null for a root frame. */
  VRT_EXPORT vrt_frame* vrt_frame_parent(vrt_frame* frame);

  /** Return a frame's raw stack Location, or zero for a null frame. */
  VRT_EXPORT uint64_t vrt_frame_id(const vrt_frame* frame);

  /** Return the function currently associated with a frame. */
  VRT_EXPORT const vrt_func* vrt_frame_func(const vrt_frame* frame);

#if defined(__cplusplus)
}
#endif
