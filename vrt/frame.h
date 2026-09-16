#pragma once

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
    uint64_t frame_id = 0;
    const Func* func = nullptr;
    uint64_t raise_target = 0;
    std::jmp_buf raise_continuation{};
  };
}
