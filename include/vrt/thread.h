#pragma once

#include "export.h"
#include "types.h"

#if defined(__cplusplus)
extern "C"
{
#endif

  /** Return the logical thread bound to this native thread, or null. */
  VRT_EXPORT vrt_thread* vrt_thread_current(void);

  /** Return the current logical frame, or null when there is none. */
  VRT_EXPORT vrt_frame* vrt_thread_current_frame(void);

#if defined(__cplusplus)
}
#endif
