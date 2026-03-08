#include "region.h"

#include "region_arena.h"
#include "region_rc.h"
#include "thread.h"
#include "value.h"

#include <cstdlib>
#include <iostream>

namespace vbci
{
  Region* Region::create(RegionType type, size_t frame_depth)
  {
    switch (type)
    {
      case RegionType::RegionArena:
        {
          auto result = new RegionArena(frame_depth);
          return result;
        }

      case RegionType::RegionRC:
        {
          auto result = new RegionRC(frame_depth);
          return result;
        }

      default:
        Value::error(Error::UnknownRegionType);
    }
  }
}
