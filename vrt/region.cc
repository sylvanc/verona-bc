#include "region.h"

#include "frame.h"

#include <exception>
#include <limits>
#include <new>

namespace
{
  [[noreturn]] void invalid_region_state()
  {
    std::terminate();
  }
}

bool vrt_region::is_frame_local() const
{
  return frame_depth != 0;
}

namespace vrt
{
  vrt_region* create_region(vrt_region_type type, uintptr_t frame_depth)
  {
    if ((type != VRT_REGION_RC) && (type != VRT_REGION_ARENA))
      invalid_region_state();

    auto* region = new (std::nothrow) vrt_region{type, frame_depth};
    if (region == nullptr)
      std::terminate();

    return region;
  }

  vrt_region* frame_region(vrt_frame* frame)
  {
    if (frame == nullptr)
      invalid_region_state();

    if (frame->region != nullptr)
      return frame->region;

    uintptr_t depth = 1;
    if (frame->parent != nullptr)
    {
      auto* parent_region = frame->parent->region;
      if (
        (parent_region == nullptr) || !parent_region->is_frame_local() ||
        (parent_region->type != VRT_REGION_RC) ||
        (parent_region->frame_depth == std::numeric_limits<uintptr_t>::max()))
        invalid_region_state();

      depth = parent_region->frame_depth + 1;
    }

    frame->region = create_region(VRT_REGION_RC, depth);
    return frame->region;
  }

  void destroy_region(vrt_region* region)
  {
    delete region;
  }

  void destroy_frame_region(vrt_frame* frame)
  {
    if ((frame == nullptr) || (frame->region == nullptr))
      invalid_region_state();

    auto* region = frame->region;
    frame->region = nullptr;
    destroy_region(region);
  }
}
