#pragma once

#include "ident.h"
#include "vbci.h"

#include <unordered_set>

namespace vbci
{
  struct Region
  {
    std::unordered_set<Region*> children;
    Region* parent;
    RC stack_rc;
    RegionType r_type;
    bool readonly;

    Region(RegionType type)
    : parent(nullptr), stack_rc(1), r_type(type), readonly(false)
    {}

    static Region* create(RegionType type)
    {
      return new Region(type);
    }

    bool enable_rc()
    {
      return !readonly && (r_type == RegionType::RegionRC);
    }

    void stack_inc()
    {
      if (!readonly)
        stack_rc++;
    }

    void stack_dec()
    {
      if (!readonly)
        stack_rc--;
    }
  };
}
