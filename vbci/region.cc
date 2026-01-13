#include "region.h"

#include "region_arena.h"
#include "region_rc.h"
#include "thread.h"
#include "value.h"

#include <cstdlib>
#include <iostream>

namespace vbci
{
  Region* Region::create(RegionType type)
  {
    switch (type)
    {
      case RegionType::RegionArena:
        {
          auto result = new RegionArena();
          return result;
        }

      case RegionType::RegionRC:
        {
          auto result = new RegionRC();
          return result;
        }

      default:
        Value::error(Error::UnknownRegionType);
    }
  }
}
