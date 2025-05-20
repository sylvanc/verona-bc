#pragma once

#include "ident.h"
#include "logging.h"
#include "region.h"
#include "value.h"

namespace vbci
{
  static constexpr auto Immutable = uintptr_t(0);
  static constexpr auto Immortal = uintptr_t(-1);
  static constexpr auto StackAlloc = uintptr_t(0x1);

  static Region* to_region(Location loc)
  {
    assert((loc & StackAlloc) == 0);
    assert(loc != Immutable);
    return reinterpret_cast<Region*>(loc);
  }

  static bool no_rc(Location loc)
  {
    return (loc & StackAlloc) != 0;
  }

  static bool is_stack(Location loc)
  {
    return (loc != Immortal) && ((loc & StackAlloc) != 0);
  }

  static bool is_region(Location loc)
  {
    return (loc != Immutable) && ((loc & StackAlloc) == 0);
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
      if ((loc == Immutable) || (loc == Immortal))
        return false;

      bool stack = is_stack(loc);
      auto vloc = v.location();
      bool vstack = is_stack(vloc);

      if (stack)
      {
        // If v is in a younger frame, fail.
        if (vstack && (vloc > loc))
          return false;
      }
      else
      {
        auto r = to_region(loc);

        // Can't store if v is on the stack.
        if (vstack)
          return false;

        if (is_region(vloc))
        {
          auto vr = to_region(vloc);

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

      if (is_stack(loc) || (dst_loc == src_loc))
        return;

      if (is_region(dst_loc))
      {
        // Increment the region stack RC.
        auto dst_r = to_region(dst_loc);
        dst_r->stack_inc();

        // Clear the parent if it's in a different region.
        if (dst_loc != loc)
          dst_r->clear_parent();
      }

      if (is_region(src_loc))
      {
        auto src_r = to_region(src_loc);

        // Set the parent if it's in a different region.
        if (src_loc != loc)
          src_r->set_parent(to_region(loc));

        // Decrement the region stack RC. This can't free the region.
        src_r->stack_dec();
      }
    }

    bool base_dec(bool reg)
    {
      // Returns false if the allocation should be freed.
      if (loc == Immutable)
      {
        return --arc != 0;
      }
      else if (!no_rc(loc))
      {
        auto r = to_region(loc);
        bool ret = true;

        if (r->enable_rc())
          ret = --rc != 0;

        // If this dec comes from a register, decrement the region stack RC.
        // If the region is freed, don't try to free the object.
        if (reg && !r->stack_dec())
          ret = true;

        return ret;
      }

      return true;
    }

    void mark_immortal()
    {
      loc = Immortal;
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
      if (!is_region(loc))
        return nullptr;

      return to_region(loc);
    }

    bool sendable()
    {
      return (loc == Immortal) || (loc == Immutable) ||
        (is_region(loc) && to_region(loc)->sendable());
    }

    void inc(bool reg)
    {
      if (loc == Immutable)
      {
        arc++;
      }
      else if (!no_rc(loc))
      {
        auto r = to_region(loc);

        // If this RC inc comes from a register, increment the region stack RC.
        if (reg)
          r->stack_inc();

        if (r->enable_rc())
          rc++;
      }
    }
  };
}
