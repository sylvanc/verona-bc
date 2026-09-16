#pragma once

#include "../include/vrt/region.h"

#include <cstdint>

namespace vrt
{
  struct Frame;

  /** Internal state for the RC region owned by a logical frame. */
  struct Region
  {
    uintptr_t frame_depth = 0;
    RegionType type;

    Region(RegionType type, uintptr_t frame_depth)
    : frame_depth(frame_depth), type(type)
    {}

    bool is_frame_local() const;
  };

  Region* create_region(RegionType type, uintptr_t frame_depth = 0);
  Region* frame_region(Frame* frame);
  void destroy_region(Region* region);
  void destroy_frame_region(Frame* frame);
}
