#pragma once

#include "drag.h"
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

    void set_rc(RC v)
    {
      rc = v;
    }

    void set_arc(RC v)
    {
      arc.store(v);
    }

    void inc_arc()
    {
      arc++;
    }

    Location location() const
    {
      return loc;
    }

    void set_location(Location l)
    {
      loc = l;
    }

    // Union-find representative lookup with path compression.
    // Used by frozen SCC objects to find their SCC root.
    static Header* find(Header* h)
    {
      if (!h->loc.is_scc_ptr())
        return h;

      auto target = h->loc.scc_target();
      auto root = find(target);

      // Path compression: point directly to root.
      if (root != target)
        h->loc = Location::scc_ptr(root);

      return root;
    }

    Region* region()
    {
      if (!loc.is_region())
        return nullptr;

      return loc.to_region();
    }

    void move_region(Region* to)
    {
      loc.to_region()->remove(this);
      loc = Location(to);
      to->insert(this);
    }

    bool sendable()
    {
      if (loc.is_stack())
        return false;

      if (loc.is_region())
      {
        auto r = loc.to_region();

        if (r->is_frame_local())
        {
          // Drag a frame-local allocation to a fresh region.
          auto nr = Region::create(RegionType::RegionRC);

          if (!drag_allocation<false>(nr, this))
            return false;

          return nr->sendable();
        }

        return r->sendable();
      }

      return true;
    }

    // Increment RC for a reference held in a register or frame-local context.
    // Adjusts both object RC and region stack_rc.
    void reg_inc()
    {
      if (loc.is_scc_ptr())
      {
        find(this)->reg_inc();
        return;
      }

      field_inc();

      if (loc.is_region())
        loc.to_region()->stack_inc();
    }

    // Decrement RC for a reference held in a register or frame-local context.
    // Adjusts both object RC and region stack_rc. Collects if RC hits 0.
    void reg_dec()
    {
      if (loc.is_scc_ptr())
      {
        find(this)->reg_dec();
        return;
      }

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
      if (loc.is_scc_ptr())
      {
        find(this)->field_inc();
        return;
      }

      if (loc.is_stack() || loc.is_immortal())
        return;

      if (loc.is_immutable())
      {
        arc++;
        return;
      }

      auto r = loc.to_region();

      if (!r->is_finalizing())
        rc++;
    }

    // Decrement RC for a reference held in an object/array field.
    // Adjusts object RC only — no stack_rc change. Collects if RC hits 0.
    void field_dec()
    {
      if (loc.is_scc_ptr())
      {
        find(this)->field_dec();
        return;
      }

      if (loc.is_stack() || loc.is_immortal())
        return;

      if (loc.is_immutable())
      {
        if (--arc == 0)
          collect_scc(this);

        return;
      }

      auto r = loc.to_region();

#ifdef NDEBUG
      if (!r->is_finalizing() && (--rc == 0))
#else
      // Detecting rc issues in finalization (as opposed to cyclic references)
      if ((--rc == 0) && !r->is_finalizing())
#endif
        collect(this);
    }
  };
}
