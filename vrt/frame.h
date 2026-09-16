#pragma once

#include "location.h"

#include <cstddef>
#include <cstdint>
#include <vrt/frame.h>

namespace vrt
{
  struct Region;

  struct Frame
  {
    Frame* parent = nullptr;
    Region* region = nullptr;
    size_t stack_mark = 0;
    size_t finalizer_mark = 0;
    const Func* func = nullptr;
    Location frame_id = Location::stack();
    Location raise_target = Location::stack();
  };
}
