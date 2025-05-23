#pragma once

#include "ident.h"
#include "logging.h"
#include "region.h"
#include "value.h"

namespace vbci
{
  namespace loc
  {
    static constexpr auto Stack = uintptr_t(0x1);
    static constexpr auto Immutable = uintptr_t(0x2);
    static constexpr auto Pending = uintptr_t(0x3);
    static constexpr auto Mask = uintptr_t(0x3);
    static constexpr auto Immortal = uintptr_t(-1) & ~Stack;

    inline bool no_rc(Location loc)
    {
      return ((loc & Stack) != 0) || (loc == Immortal);
    }

    inline bool is_region(Location loc)
    {
      return (loc & Mask) == 0;
    }

    inline bool is_stack(Location loc)
    {
      return (loc & Mask) == Stack;
    }

    inline bool is_immutable(Location loc)
    {
      return (loc & Mask) == Immutable;
    }

    inline bool is_pending(Location loc)
    {
      return (loc & Mask) == Pending;
    }

    inline Region* to_region(Location loc)
    {
      assert(is_region(loc));
      return reinterpret_cast<Region*>(loc);
    }

    inline Header* to_scc(Location loc)
    {
      assert(is_immutable(loc));
      return reinterpret_cast<Header*>(loc & ~Immutable);
    }
  }

  struct Header
  {
  private:
    Location loc;

    union
    {
      RC rc;
      ARC arc;
    };

    Id type_id;

  protected:
    Header(Location loc, Id type_id) : loc(loc), rc(1), type_id(type_id) {}

    bool safe_store(Value& v)
    {
      if (loc::is_immutable(loc))
        return false;

      bool stack = loc::is_stack(loc);
      auto vloc = v.location();
      bool vstack = loc::is_stack(vloc);

      if (stack)
      {
        // If v is in a younger frame, fail.
        if (vstack && (vloc > loc))
          return false;
      }
      else
      {
        auto r = loc::to_region(loc);

        // Can't store if v is on the stack.
        if (vstack)
          return false;

        if (loc::is_region(vloc))
        {
          auto vr = loc::to_region(vloc);

          // Can't store if they're in a different region that has a parent, or
          // if they're an ancestor of this region.
          if ((r != vr) && (vr->has_parent() || vr->is_ancestor(r)))
            return false;
        }
      }

      return true;
    }

    void region_store(Value& dst, Value& src)
    {
      // If this is a stack allocation, we're moving the stack RC from the
      // store location to the register (for prev), or from the register to
      // the store location (for src).
      auto dst_loc = dst.location();
      auto src_loc = src.location();

      if (loc::is_stack(loc) || (dst_loc == src_loc))
        return;

      if (loc::is_region(dst_loc))
      {
        // Increment the region stack RC.
        auto dst_r = loc::to_region(dst_loc);
        dst_r->stack_inc();

        // Clear the parent if it's in a different region.
        if (dst_loc != loc)
          dst_r->clear_parent();
      }

      if (loc::is_region(src_loc))
      {
        auto src_r = loc::to_region(src_loc);

        // Set the parent if it's in a different region.
        if (src_loc != loc)
          src_r->set_parent(loc::to_region(loc));

        // Decrement the region stack RC. This can't free the region.
        src_r->stack_dec();
      }
    }

    Header* get_scc()
    {
      assert(loc::is_immutable(loc));
      auto c = this;
      auto p = loc::to_scc(loc);

      if (!p)
        return c;

      while (true)
      {
        auto gp = loc::to_scc(p->loc);

        if (!gp)
          return p;

        // Compact the SCC chain.
        c->loc = p->loc;
        c = p;
        p = gp;
      }
    }

    bool base_dec(bool reg)
    {
      // Returns false if the allocation should be freed.
      if (loc::no_rc(loc))
        return true;

      if (loc::is_immutable(loc))
      {
        // TODO: how do we correctly free an SCC?
        return --get_scc()->arc != 0;
      }

      auto r = loc::to_region(loc);
      bool ret = true;

      if (r->enable_rc())
        ret = --rc != 0;

      // If this dec comes from a register, decrement the region stack RC.
      // If the region is freed, don't try to free the object.
      if (reg && !r->stack_dec())
        ret = true;

      return ret;
    }

    void mark_immortal()
    {
      loc = loc::Immortal;
    }

  public:
    Id get_type_id()
    {
      return type_id;
    }

    bool is_object()
    {
      return !type::is_array(type_id);
    }

    bool is_array()
    {
      return type::is_array(type_id);
    }

    Location location()
    {
      return loc;
    }

    Region* region()
    {
      if (!loc::is_region(loc))
        return nullptr;

      return loc::to_region(loc);
    }

    bool sendable()
    {
      return loc::is_immutable(loc) ||
        (loc::is_region(loc) && loc::to_region(loc)->sendable());
    }

    void inc(bool reg)
    {
      if (loc::no_rc(loc))
        return;

      if (loc::is_immutable(loc))
      {
        get_scc()->arc++;
        return;
      }

      auto r = loc::to_region(loc);

      // If this RC inc comes from a register, increment the region stack RC.
      if (reg)
        r->stack_inc();

      if (r->enable_rc())
        rc++;
    }
  };
}
