#include "region.h"

#include "failure.h"
#include "frame.h"

#include <limits>
#include <new>

bool vrt::Region::is_frame_local() const
{
  return frame_depth != 0;
}

namespace vrt
{
  Region* create_region(RegionType type, uintptr_t frame_depth)
  {
    if ((type != VRT_REGION_RC) && (type != VRT_REGION_ARENA))
      fail(Failure::invalid_region_state);

    auto* region = new (std::nothrow) Region{type, frame_depth};
    if (region == nullptr)
      fail(Failure::out_of_memory);

    return region;
  }

  Region* frame_region(Frame* frame)
  {
    if (frame == nullptr)
      fail(Failure::invalid_region_state);

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
        fail(Failure::invalid_region_state);

      depth = parent_region->frame_depth + 1;
    }

    frame->region = create_region(VRT_REGION_RC, depth);
    return frame->region;
  }

  void destroy_region(Region* region)
  {
    delete region;
  }

  void destroy_frame_region(Frame* frame)
  {
    if ((frame == nullptr) || (frame->region == nullptr))
      fail(Failure::invalid_region_state);

    auto* region = frame->region;
    frame->region = nullptr;
    destroy_region(region);
  }
}
