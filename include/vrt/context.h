#pragma once

#include "export.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

  /** Logical execution state for one Verona invocation. */
  typedef struct vrt_thread vrt_thread;

  /** Logical Verona function frame. */
  typedef struct vrt_frame vrt_frame;

  /** Static metadata describing a generated Verona function. */
  typedef struct vrt_function_descriptor
  {
    uint64_t id;
    const char* name;
  } vrt_function_descriptor;

  /** Return the logical thread bound to this native thread, or null. */
  VRT_EXPORT vrt_thread* vrt_thread_current(void);

  /** Return the current logical frame, or null when there is none. */
  VRT_EXPORT vrt_frame* vrt_thread_current_frame(void);

  /**
   * Enter a function using the logical thread bound to this native thread.
   *
   * function may be null. A non-null descriptor must remain alive until the
   * frame is left or rebound by a tailcall.
   *
   * An ordinary entry pushes a new logical frame. The entry immediately after
   * vrt_frame_prepare_tailcall reuses the prepared current frame instead.
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
   * The thread bound to this native thread must have a current frame and no
   * pending tailcall.
   */
  VRT_EXPORT void vrt_frame_leave(void);

  /**
   * Rebind the current logical frame for a tailcall.
   *
   * The current frame keeps its identity, parent, region, and teardown
   * boundaries. The next call to vrt_frame_enter consumes the pending transfer
   * and reuses that frame.
   *
   * Lowering is responsible for moving arguments and releasing other locals
   * before calling this function. function may be null; a non-null descriptor
   * must remain alive until the frame is left or rebound again.
   */
  VRT_EXPORT void
  vrt_frame_prepare_tailcall(const vrt_function_descriptor* function);

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
