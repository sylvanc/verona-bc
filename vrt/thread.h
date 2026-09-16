#pragma once

#include "location.h"

#include <cstdint>
#include <vrt/thread.h>

namespace vrt
{
  struct Thread
  {
    /** Return the logical thread bound to the calling native thread. */
    static Thread& get();

    /** Return the bound logical thread, or null if it is uninitialized. */
    static Thread* try_get();

    /** Raise a type-erased value through an older active stack Location. */
    [[noreturn]] void raise(uint64_t value, Location target);

    Frame* frame = nullptr;
  };
}
