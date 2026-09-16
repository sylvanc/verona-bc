#pragma once

#include "export.h"
#include "frame.h"

#if defined(__cplusplus)
namespace vrt
{
  struct Thread;
}

/** Logical execution state for one Verona invocation. */
using vrt_thread = vrt::Thread;

extern "C"
{
#else
/** Logical execution state for one Verona invocation. */
typedef struct vrt_thread vrt_thread;
#endif

  /** Return the logical thread bound to this native thread, or null. */
  VRT_EXPORT vrt_thread* vrt_thread_current(void);

  /** Return the current logical frame, or null when there is none. */
  VRT_EXPORT vrt_frame* vrt_thread_current_frame(void);

#if defined(__cplusplus)
}
#endif
