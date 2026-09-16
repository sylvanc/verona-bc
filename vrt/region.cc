#include "region.h"

#include "failure.h"
#include "frame.h"

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

    frame->region = create_region(
      VRT_REGION_RC, frame->frame_id.stack_index() + 1);
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
