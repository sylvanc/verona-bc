#pragma once

#include "array.h"
#include "object.h"
#include "program.h"
#include "region.h"
#include "region_arena.h"
#include "region_rc.h"

namespace vbci
{
  void Region::trace_fn(auto&& fn) const
  {
    switch (type)
    {
      case RegionType::RegionRC:
        static_cast<const RegionRC*>(this)->trace_fn(fn);
        break;

      case RegionType::RegionArena:
        static_cast<const RegionArena*>(this)->trace_fn(fn);
        break;
    }
  }

  void Region::for_each_header(auto&& fn) const
  {
    switch (type)
    {
      case RegionType::RegionRC:
        static_cast<const RegionRC*>(this)->for_each_header(fn);
        break;

      case RegionType::RegionArena:
        static_cast<const RegionArena*>(this)->for_each_header(fn);
        break;
    }
  }

  void RegionRC::trace_fn(auto&& fn) const
  {
    auto& program = Program::get();

    for (auto h : headers)
    {
      if (program.is_array(h->get_type_id()))
        static_cast<Array*>(h)->trace_fn(fn);
      else
        static_cast<Object*>(h)->trace_fn(fn);
    }
  }

  void RegionRC::for_each_header(auto&& fn) const
  {
    for (auto h : headers)
      fn(h);
  }
}
