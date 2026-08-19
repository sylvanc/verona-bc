#pragma once

#include <cstdint>
#include <vrt/frame.h>
#include <vrt/thread.h>

struct vrt_thread
{
  vrt_frame* current_frame = nullptr;
  const vrt_function_descriptor* tailcall_target = nullptr;
  uint64_t next_frame_id = 1;
  bool tailcall_pending = false;
};

namespace vrt
{
  /** Return the logical thread bound to the calling native thread. */
  vrt_thread* current_thread();
}
