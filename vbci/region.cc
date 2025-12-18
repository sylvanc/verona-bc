#include "region.h"

#include "region_arena.h"
#include "region_rc.h"
#include "thread.h"
#include "value.h"

#include <cstdlib>
#include <iostream>

namespace vbci
{
  namespace
  {
    struct CownParentRegion final : Region
    {
      Object* object(Class&) override
      {
        abort();
        return nullptr;
      }

      Array* array(uint32_t, size_t) override
      {
        abort();
        return nullptr;
      }

      void rfree(Header*) override {}
      void insert(Header*) override {}
      void remove(Header*) override {}
      bool enable_rc() override
      {
        return false;
      }

      void free_contents() override {}
    };
  }

  Region* Region::cown_region()
  {
    static CownParentRegion instance;
    return &instance;
  }

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
