#pragma once

#include "region_rc.h"

namespace vbci
{
  struct RegionArena : public RegionRC
  {
    friend struct Region;

  protected:
    RegionArena() : RegionRC()
    {
      LOG(Trace) << "Created RegionArena @" << this;
    }

    bool is_finalizing()
    {
      return true;
    }
  };
}
