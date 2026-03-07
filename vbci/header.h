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

    // Returns true if the object needs deallocating.
    template<bool needs_stack_rc>
    bool dec_no_dealloc()
    {
      // Returns false if the allocation should be freed.
      if (loc.no_rc())
        return false;

      if (loc.is_immutable())
        return --arc == 0;

      auto r = loc.to_region();
      bool ret = false;

      if (r->enable_rc())
        ret = --rc == 0;

      if (!needs_stack_rc)
        return ret;

      return r->stack_dec() && ret;
    }

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

    template<bool needs_stack_rc>
    void inc()
    {
      if (loc.no_rc())
        return;

      if (loc.is_immutable())
      {
        arc++;
        return;
      }

      auto r = loc.to_region();

      // If this RC inc comes from a register or frame local object,
      // increment the region stack RC.
      if (needs_stack_rc)
        r->stack_inc();

      if (r->enable_rc())
        rc++;
    }

    template<bool needs_stack_rc>
    void dec()
    {
      if (dec_no_dealloc<needs_stack_rc>())
        // Queue object/array for deallocation
        collect(this);
    }

    void stack_inc()
    {
      if (loc.is_region())
        loc.to_region()->stack_inc();
    }

    void stack_dec()
    {
      if (loc.is_region())
        loc.to_region()->stack_dec();
    }
  };
}
