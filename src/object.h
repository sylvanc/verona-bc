#pragma once

#include "ident.h"
#include "region.h"
#include "value.h"

namespace vbci
{
  static constexpr auto Immutable = uintptr_t(0);
  static constexpr auto Immortal = uintptr_t(-1);
  static constexpr auto StackAlloc = uintptr_t(0x1);

  // An object header is 16 bytes on a 64 bit system, 12 bytes on a 32 bit
  // system.
  struct Object
  {
    union
    {
      RC rc;
      ARC arc;
    };

    ClassId class_id;
    Location loc;
    Value fields[0];

    static Object* create(ClassId class_id, Location loc, size_t fields);

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

    Region* region()
    {
      return region(loc);
    }

    bool no_rc()
    {
      return no_rc(loc);
    }

    bool is_immutable()
    {
      return is_immutable(loc);
    }

    bool is_stack()
    {
      return is_stack(loc);
    }

    bool is_region()
    {
      return is_region(loc);
    }

    void inc()
    {
      if (loc == Immutable)
      {
        arc++;
      }
      else if (no_rc())
      {
        // Do nothing.

        // TODO: no RC for stack alloc?
        // if so, need an actual bump allocator
        // segmented stack, to avoid over-allocation
      }
      else
      {
        // RC inc comes from new values in registers. As such, it's paired with
        // a stack RC increment for the containing region.
        region()->stack_inc();

        if (region()->enable_rc())
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
      else if (no_rc())
      {
        // Do nothing.
      }
      else
      {
        // RC dec comes from invalidating values in registers. As such, it's
        // paired with a stack RC decrement for the containing region.

        // TODO: what if the region is freed here?
        region()->stack_dec();

        if (region()->enable_rc())
        {
          // TODO: free at zero
          rc--;
        }
      }
    }

    Value store(FieldIdx idx, Value& v)
    {
      // TODO: type_check
      if (is_immutable())
        throw Value(Error::BadStoreTarget);

      bool stack = is_stack();
      auto vloc = v.location();
      bool vstack = is_stack(vloc);

      if (stack)
      {
        // If v is in a younger frame, fail.
        if (vstack && (vloc > loc))
          throw Value(Error::BadStore);
      }
      else
      {
        auto r = region();

        // Can't store if we're readonly or if v is on the stack.
        if (r->readonly || vstack)
          throw Value(Error::BadStore);

        if (is_region(vloc))
        {
          auto vr = region(vloc);

          // Can't store if they're readonly, or if they're in a different
          // region that has a parent, or if they're an ancestor of this region.
          if (vr->readonly || ((r != vr) && (vr->parent || vr->is_ancestor(r))))
            throw Value(Error::BadStore);
        }
      }

      auto& field = fields[idx];
      auto prev = std::move(field);
      auto ploc = prev.location();

      if (ploc != vloc)
      {
        if (is_region(ploc))
        {
          // Increment the region stack RC.
          region(ploc)->stack_inc();

          // Clear the parent if it's in a different region.
          if (!stack && (ploc != loc))
            region(ploc)->clear_parent();
        }

        if (is_region(vloc))
        {
          // Decrement the region stack RC.
          region(vloc)->stack_dec();

          // Set the parent if it's in a different region.
          if (!stack && (vloc != loc))
            region(vloc)->set_parent(region());
        }
      }

      field = std::move(v);
      return prev;
    }
  };
}
