#include "region.h"

#include "region_rc.h"
#include "value.h"

namespace vbci
{
  Region* Region::create(RegionType type)
  {
    switch (type)
    {
      case RegionType::RegionRC:
        return new RegionRC();

      default:
        throw Value(Error::UnknownRegionType);
    }
  }
}
