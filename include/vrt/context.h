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

  /** Create an empty logical Verona thread. */
  VRT_EXPORT vrt_thread* vrt_thread_create(void);

  /**
   * Destroy a logical Verona thread and any frames still owned by it.
   *
   * Passing a null pointer has no effect.
   */
  VRT_EXPORT void vrt_thread_destroy(vrt_thread* thread);

  /** Return the current frame, or null when the thread has no frames. */
  VRT_EXPORT vrt_frame* vrt_thread_current_frame(vrt_thread* thread);

  /**
   * Enter a function by pushing a new logical frame.
   *
   * function may be null. A non-null descriptor must remain alive until the
   * frame is left or rebound by a tailcall.
   *
   * Returns null if thread is null or the frame cannot be allocated.
   */
  VRT_EXPORT vrt_frame*
  vrt_frame_enter(vrt_thread* thread, const vrt_function_descriptor* function);

  /**
   * Leave and destroy the current logical frame.
   *
   * The frame must be the current frame of thread.
   */
  VRT_EXPORT void vrt_frame_leave(vrt_thread* thread, vrt_frame* frame);

  /**
   * Rebind the current logical frame for a tailcall.
   *
   * The frame keeps its identity, parent, region, and teardown boundaries.
   * Lowering is responsible for moving arguments and releasing other locals
   * before calling this function. function may be null; a non-null descriptor
   * must remain alive until the frame is left or rebound again.
   */
  VRT_EXPORT void vrt_frame_prepare_tailcall(
    vrt_thread* thread,
    vrt_frame* frame,
    const vrt_function_descriptor* function);

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
