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

    bool enable_rc()
    {
      return false;
    }
  };
}
