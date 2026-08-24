#pragma once

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <vrt/frame.h>

struct vrt_region;

struct vrt_frame
{
  vrt_frame* parent = nullptr;
  vrt_region* region = nullptr;
  size_t stack_mark = 0;
  size_t finalizer_mark = 0;
  uint64_t frame_id = 0;
  const vrt_function_descriptor* function = nullptr;
  uint64_t raise_target = 0;
  std::jmp_buf raise_continuation{};
};
