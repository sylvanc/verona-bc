#pragma once

#include <cstdint>
#include <vrt/frame.h>
#include <vrt/thread.h>

namespace vrt
{
  struct Thread
  {
    Frame* frame = nullptr;
    uint64_t next_frame_id = 1;
    uint64_t pending_raise_value = 0;
    Frame* pending_raise_target = nullptr;
    bool raise_pending = false;
  };

  /** Return the logical thread bound to the calling native thread. */
  Thread* current_thread();
}
