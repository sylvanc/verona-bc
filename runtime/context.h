#pragma once

#include <cstddef>
#include <cstdint>
#include <vrt/context.h>

struct vrt_region;

struct vrt_thread
{
  vrt_frame* current_frame = nullptr;
  uint64_t next_frame_id = 1;
};

struct vrt_frame
{
  vrt_thread* thread = nullptr;
  vrt_frame* parent = nullptr;
  vrt_region* region = nullptr;
  size_t stack_mark = 0;
  size_t finalizer_mark = 0;
  uint64_t frame_id = 0;
  const vrt_function_descriptor* function = nullptr;
  uint64_t raise_target = 0;
};
