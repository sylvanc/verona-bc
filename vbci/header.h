#pragma once

#include "ident.h"
#include "location.h"
#include "region.h"
#include "register.h"
#include "value.h"

#include <limits>

namespace vbci
{
  struct Header
  {
  public:
    static constexpr uint32_t StackSentinelTypeId =
      std::numeric_limits<uint32_t>::max();

  private:
    Location loc;

    union
    {
      RC rc;
      ARC arc;
    };

    uint32_t type_id;

  protected:
    Header(Location loc, uint32_t type_id) : loc(loc), rc(1), type_id(type_id)
    {}

    void mark_immortal()
    {
      loc = Location::immortal();
    }

  public:
    uint32_t get_type_id() const
    {
      return type_id;
    }

    RC get_rc()
    {
      return rc;
    }

    Location location() const
    {
      return loc;
    }

    Region* region()
    {
      if (!loc.is_region_or_frame_local() || loc.no_rc())
        return nullptr;

      return loc.to_region();
    }

    void move_region(Location to_loc, Region* to)
    {
      assert(to_loc.is_region_or_frame_local());

      loc.to_region()->remove(this);
      loc = to_loc;
      to->insert(this);
    }

    bool sendable()
    {
      if (loc.is_stack())
        return false;

      if (loc.is_frame_local())
      {
        // Drag a frame-local allocation to a region.
        auto nr = Region::create(RegionType::RegionRC);

        if (!drag_allocation<false>(Location(nr), this))
          return false;

        return nr->sendable();
      }

      if (loc.is_region())
        return loc.to_region()->sendable();

      return true;
    }

    // Increment RC for a reference held in a register or frame-local context.
    // Adjusts both object RC and region stack_rc.
    void reg_inc()
    {
      field_inc();

      if (loc.is_region())
        loc.to_region()->stack_inc();
    }

    // Decrement RC for a reference held in a register or frame-local context.
    // Adjusts both object RC and region stack_rc. Collects if RC hits 0.
    void reg_dec()
    {
      // If stack_dec returns false, the region has been freed, so we can
      // return early without doing anything else.
      if (loc.is_region() && !loc.to_region()->stack_dec())
        return;

      field_dec();
    }

    // Increment RC for a reference held in an object/array field.
    // Adjusts object RC only — no stack_rc change.
    void field_inc()
    {
      if (loc.no_rc())
        return;

      if (loc.is_immutable())
      {
        arc++;
        return;
      }

      auto r = loc.to_region();

      if (r->enable_rc())
        rc++;
    }

    // Decrement RC for a reference held in an object/array field.
    // Adjusts object RC only — no stack_rc change. Collects if RC hits 0.
    void field_dec()
    {
      if (loc.no_rc())
        return;

      if (loc.is_immutable())
      {
        if (--arc == 0)
          collect(this);
        return;
      }

      auto r = loc.to_region();

      if (r->enable_rc() && (--rc == 0))
        collect(this);
    }
  };
}
