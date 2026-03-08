#pragma once

#include "region_rc.h"

namespace vbci
{
  struct RegionArena : public RegionRC
  {
    friend struct Region;

  protected:
    RegionArena(size_t frame_depth = 0) : RegionRC(frame_depth)
    {
      LOG(Trace) << "Created RegionArena @" << this;
    }

    bool is_finalizing()
    {
      return true;
    }
  };
}
