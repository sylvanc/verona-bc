#pragma once

#include "region_rc.h"

namespace vbci
{
  struct RegionArena : public RegionRC
  {
    friend struct Region;

  protected:
    RegionArena(RegionType type, size_t frame_depth)
    : RegionRC(type, frame_depth)
    {
      LOG(Trace) << "Created RegionArena @" << this;
    }

    bool is_finalizing()
    {
      return true;
    }
  };
}
