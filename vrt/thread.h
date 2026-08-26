#pragma once

#include <cstdint>
#include <vrt/frame.h>
#include <vrt/thread.h>

struct vrt_thread
{
  vrt_frame* current_frame = nullptr;
  uint64_t next_frame_id = 1;
  uint64_t pending_raise_value = 0;
  vrt_frame* pending_raise_target = nullptr;
  bool raise_pending = false;
};

namespace vrt
{
  /** Return the logical thread bound to the calling native thread. */
  vrt_thread* current_thread();
}
