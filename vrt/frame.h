#pragma once

#include "location.h"

#include <csetjmp>
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
    Location frame_id = Location::stack();
    const Func* func = nullptr;
    Location raise_target = Location::stack();
    std::jmp_buf raise_continuation{};
  };
}
