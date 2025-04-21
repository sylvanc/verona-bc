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

  struct Header
  {
  private:
    Location loc;

    union
    {
      RC rc;
      ARC arc;
    };

    static Region* region(Location loc)
    {
      assert((loc & StackAlloc) == 0);
      assert(loc != Immutable);
      return reinterpret_cast<Region*>(loc);
    }

    static bool no_rc(Location loc)
    {
      return (loc & StackAlloc) != 0;
    }

    static bool is_immutable(Location loc)
    {
      return (loc == Immutable) || (loc == Immortal);
    }

    static bool is_stack(Location loc)
    {
      return (loc != Immortal) && ((loc & StackAlloc) != 0);
    }

    static bool is_region(Location loc)
    {
      return (loc != Immutable) && ((loc & StackAlloc) == 0);
    }

    bool safe_store(Value& v)
    {
      if (is_immutable(loc))
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
        auto r = region(loc);

        // Can't store if v is on the stack.
        if (vstack)
          return false;

        if (is_region(vloc))
        {
          auto vr = region(vloc);

          // Can't store if they're in a different region that has a parent, or
          // if they're an ancestor of this region.
          if ((r != vr) && (vr->parent || vr->is_ancestor(r)))
            return false;
        }
      }

      return true;
    }

  protected:
    Header(Location loc) : loc(loc), rc(1) {}

    Value base_store(ArgType arg_type, Value& dst, Value& src)
    {
      if (!safe_store(src))
        throw Value(Error::BadStore);

      auto stack = is_stack(loc);
      auto dst_loc = dst.location();
      auto src_loc = src.location();

      // If this is a stack allocation, we're moving the stack RC from the
      // store location to the register (for prev), or from the register to
      // the store location (for src), so no action is needed.
      // If prev and src are in the same region, we don't need to do anything.
      if (!stack && (dst_loc != src_loc))
      {
        if (is_region(dst_loc))
        {
          // Increment the region stack RC.
          auto dst_r = region(dst_loc);
          dst_r->stack_inc();

          // Clear the parent if it's in a different region.
          if (dst_loc != loc)
            dst_r->clear_parent();
        }

        if (is_region(src_loc))
        {
          auto src_r = region(src_loc);

          // Set the parent if it's in a different region.
          if (src_loc != loc)
            src_r->set_parent(region(loc));

          // Decrement the region stack RC.
          src_r->stack_dec();
        }
      }

      return dst.swap(arg_type, stack, src);
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
        auto r = region(loc);
        bool ret = true;

        if (r->enable_rc())
          ret = --rc != 0;

        // If this dec comes from a register, decrement the region stack RC.
        if (reg)
        {
          // TODO: when can we collect the region?
          r->stack_dec();
        }

        return ret;
      }

      return true;
    }

  public:
    Location location()
    {
      return loc;
    }

    Region* region()
    {
      if (!is_region(loc))
        throw Value(Error::BadAllocTarget);

      return region(loc);
    }

    void inc(bool reg)
    {
      if (loc == Immutable)
      {
        arc++;
      }
      else if (!no_rc(loc))
      {
        auto r = region(loc);

        // If this RC inc comes from a register, increment the region stack RC.
        if (reg)
          r->stack_inc();

        if (r->enable_rc())
          rc++;
      }
    }
  };
}
