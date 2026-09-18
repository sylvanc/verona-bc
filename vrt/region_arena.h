#pragma once

#include "region_rc.h"

namespace vrt
{
  /** Arena region with RegionRC allocation tracking and bulk reclamation. */
  struct RegionArena : public RegionRC
  {
    friend struct Region;

  protected:
    RegionArena(RegionType type, uintptr_t frame_depth = 0)
    : RegionRC(type, frame_depth)
    {}

  public:
    bool is_arena() const override
    {
      return true;
    }
  };
}
