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
    union
    {
      RC rc;
      ARC arc;
    };

    Location loc;

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
    Header(Location loc) : rc(1), loc(loc) {}

    Value base_store(ArgType arg_type, Value& dst, Value& src)
    {
      if (!safe_store(src))
        throw Value(Error::BadStore);

      auto stack = is_stack(loc);
      auto prev = std::move(dst);
      auto ploc = prev.location();
      auto vloc = src.location();

      if (ploc != vloc)
      {
        if (is_region(ploc))
        {
          // Increment the region stack RC.
          auto pr = region(ploc);
          pr->stack_inc();

          // Clear the parent if it's in a different region.
          if (!stack && (ploc != loc))
            pr->clear_parent();
        }

        if (is_region(vloc))
        {
          // Decrement the region stack RC.
          auto vr = region(vloc);
          vr->stack_dec();

          // Set the parent if it's in a different region.
          if (!stack && (vloc != loc))
          {
            auto r = region(loc);
            vr->set_parent(r);
          }
        }
      }

      if (arg_type == ArgType::Move)
        dst = std::move(src);
      else
        dst = src;

      return prev;
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

    void inc()
    {
      if (loc == Immutable)
      {
        arc++;
      }
      else if (!no_rc(loc))
      {
        // RC inc comes from new values in registers. As such, it's paired with
        // a stack RC increment for the containing region.
        auto r = region(loc);
        r->stack_inc();

        if (r->enable_rc())
          rc++;
      }
    }

    void dec()
    {
      if (loc == Immutable)
      {
        // TODO: free at zero
        arc--;
      }
      else if (!no_rc(loc))
      {
        // RC dec comes from invalidating values in registers. As such, it's
        // paired with a stack RC decrement for the containing region.
        auto r = region(loc);

        if (r->enable_rc())
        {
          if (--rc == 0)
          {
            logging::Debug() << "Free " << this << std::endl;
            // TODO: finalize, decrement fields
            // free(this);
          };
        }

        r->stack_dec();
      }
    }
  };
}
