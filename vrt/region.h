#pragma once

#include "../include/vrt/region.h"

#include <cstdint>

struct vrt_frame;

/** Internal state for the RC region owned by a logical frame. */
struct vrt_region
{
  uintptr_t frame_depth = 0;
  vrt_region_type type;

  vrt_region(vrt_region_type type, uintptr_t frame_depth)
  : frame_depth(frame_depth), type(type)
  {}

  bool is_frame_local() const;
};

namespace vrt
{
  vrt_region* create_region(vrt_region_type type, uintptr_t frame_depth = 0);
  vrt_region* frame_region(vrt_frame* frame);
  void destroy_region(vrt_region* region);
  void destroy_frame_region(vrt_frame* frame);
}
