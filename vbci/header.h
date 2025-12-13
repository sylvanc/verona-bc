#pragma once

#include "ident.h"
#include "location.h"
#include "logging.h"
#include "region.h"
#include "register.h"
#include "value.h"

namespace vbci
{
  struct Header
  {
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

    template<bool is_move>
    bool add_region_reference(const Value& next) const
    {
      constexpr bool is_copy = !is_move;
      auto nloc = next.location();

      if (loc::is_immutable(nloc))
        return true;

      if (loc::is_stack(loc))
      {
        if (loc::is_stack(nloc))
          // Older frames can't point to newer frames.
          return (loc >= nloc);

        assert(loc::is_region(nloc));
        auto nr = loc::to_region(nloc);

        // Older frames can't point to newer frames.
        if (nr->is_frame_local())
        {
          if (loc < nr->get_parent())
            return false;

          // Stack to frame local doesn't need a stack rc.
          return true;
        }

        if constexpr (is_copy)
          // Copy so need to increase stack RC.
          nr->stack_inc();
        return true;
      }

      assert(loc::is_region(loc));

      if (loc::is_stack(nloc))
        // No region, even a frame-local one, can point to the stack.
        return false;

      assert(loc::is_region(nloc));
      auto r = loc::to_region(loc);
      auto nr = loc::to_region(nloc);

      if (r->is_frame_local())
      {
        if (!nr->is_frame_local())
        {
          if constexpr (is_copy)
          {
            // Copy so need to increase stack RC.
            nr->stack_inc();
          }
          return true;
        }
      }

      // Creating an interior region reference is fine and has no accounting.
      if (r == nr)
      {
        // For the move case, we need to remove the stack rc as the register is
        // going away, and this new reference is internal, so not tracked with
        // stack rc.
        if constexpr (is_move)
          nr->stack_dec();
        return true;
      }

      if (nr->is_frame_local())
      {
        // Drag a frame-local allocation to a region.
        auto success = drag_allocation(r, next.get_header());
        if (!success)
          return false;
        // TODO Should perhaps move this inside drag_allocation?
        if (is_move && !r->is_frame_local())
          r->stack_dec();
        return true;
      }

      if (nr->has_parent())
      {
        // Regions can only have a single entry point,
        // but frame-local regions are allowed.
        return false;
      }
      else if (nr->is_ancestor_of(r))
      {
        // Regions can't form cycles.
        return false;
      }

      // At this point, the write barrier is satisfied.
      // If loc is the stack or a frame-local region, no action is needed
      // as the stack RC was already provided by the register that was passed
      // in.
      assert(!loc::is_stack(loc));

      assert(loc::is_region(nloc));

      // Set the parent
      // it's in a different region.
      assert(r != nr);
      assert(!r->is_frame_local());
      nr->set_parent(r);

      // For the move case, decrement the region stack RC. This can't free the
      // region as we just parented it.
      if constexpr (is_move)
        nr->stack_dec();
      return true;
    }

    /**
     * @brief Remove a reference from the region meta-data.
     *
     * to_register: Indicates whether the value is going into a register.
     * This will then perform the required stack RC increment.
     *
     * !to_register: Indicates the reference is actually going to be removed.
     * This causes the stack RC to be decremented if needed, and the
     * region parent to be cleared, and possibly freed.
     *
     * The underlying value will also be decrefed in the !to_register case.
     *
     * Returns false if the region has been freed.
     */
    template<bool to_register = true>
    bool remove_region_reference(Location ploc) const
    {
      if (!loc::is_region(ploc))
        return true;

      assert(!loc::is_stack(ploc));

      auto pr = loc::to_region(ploc);
      auto r = loc::to_region(loc);

      // Check if the region is frame-local and hence does not need unparenting.
      if (pr->is_frame_local())
        // We don't need stack rc for frame_local regions either.
        return true;

      if (r->is_frame_local())
      {
        // This is being removed from an object/array that is frame local, and
        // will not land in register we need to remove its stack rc.
        if constexpr (!to_register)
          return pr->stack_dec();
        return true;
      }

      // This is being removed from an object/array, and will land in a
      // register, so we need to increment the stack RC to account for that.
      if constexpr (to_register)
        pr->stack_inc();

      // Internal edges cannot be parents
      if (pr == r)
        return true;

      bool cleared = pr->clear_parent();
      if constexpr (!to_register)
      {
        if (cleared)
          pr->free_region();
      }
      else
        // Invariant: when moving to a register, the region can't be freed
        assert(!cleared);
      return !cleared;
    }

    // This is used when the write barrier fails, and we need to undo the
    // effects of `remove_region_reference`.  This can only undo the
    // `to_register == true` case for `remove_region_reference`.
    void restore_reference(Location ploc) const
    {
      if (!loc::is_region(ploc))
        return;

      auto pr = loc::to_region(ploc);
      auto r = loc::to_region(loc);

      if (pr->is_frame_local())
        return;

      if (r->is_frame_local())
      {
        return;
      }

      // We need to reverse the order with respect to remove_region_reference
      // So reparent if we unparented, and then remove the stack inc.
      if (pr != r)
        r->set_parent(loc::to_region(loc));

      pr->stack_dec();
    }

    template<bool is_move, bool no_previous = false>
    Register store(void* addr, ValueType t, Reg<is_move> next) const
    {
      if (loc::is_immutable(loc))
        Value::error(Error::BadStoreTarget);

      auto nloc = next.location();

      Register prev;
      if constexpr (!no_previous)
      {
        // Load the previous value.
        prev = Value::from_addr(t, addr);
        // prev is not correctly reference counted yet.
        // Write barrier operations are required to establish heap invariants.
        // prev is missing classic RC, and stack RC.

        auto ploc = prev.location();

        // If we are moving a pointer to a different object in the same region,
        // then we we can just need to perform a stack_inc, and if it is a copy,
        // a normal RC inc.
        if (ploc == nloc)
        {
          // Now safe to perform the write.
          next.template to_addr<is_move>(t, addr);
          // We have overwritten the previous value, so the classic RC invariant
          // is reestablished.

          if (loc::is_region(ploc) && !loc::to_region(ploc)->is_frame_local())
          {
            if constexpr (!is_move)
              loc::to_region(ploc)->stack_inc();
          }
          return prev;
        }

        remove_region_reference(ploc);
        // The stack RC invariant for prev now holds, but it is still
        // missing a classic RC.  Overwriting the value will establish this,
        // but in the case of failure we need to invalidate prev to avoid
        // it removing a classic RC it was never entitled to.
      }

      if (!add_region_reference<is_move>(next))
      {
        // Failed to add references, need to restore prev's RCs.
        if constexpr (!no_previous)
        {
          auto ploc = prev.location();
          restore_reference(ploc);
        }

        Value::error(Error::BadStore);
      }

      if constexpr (!is_move)
      {
        // TODO should add_region_reference have already done this?
        next.template inc<false>();
      }
      // Now safe to perform the write.
      next.template to_addr<is_move>(t, addr);
      // We have overwritten the previous value, so the classic RC invariant is
      // reestablished for prev.
      return prev;
    }

    void field_drop(Value& prev)
    {
      auto ploc = prev.location();
      auto dealloc = dec_no_dealloc<false>();
      auto region_dealloc = remove_region_reference<false>(ploc);
      if (dealloc && !region_dealloc)
      {
        // Only collect the allocation, if the region isn't already going away.
        collect(this);
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

    // Returns true if the object needs deallocating.
    template<bool is_register>
    bool dec_no_dealloc()
    {
      // Returns false if the allocation should be freed.
      if (loc::no_rc(loc))
        return false;

      if (loc::is_immutable(loc))
      {
        // TODO: how do we correctly free an SCC?
        return --get_scc()->arc == 0;
      }

      auto r = loc::to_region(loc);
      bool ret = false;

      if (r->enable_rc())
        ret = --rc == 0;

      if (!is_register)
        return ret;

      return r->stack_dec() && ret;
    }

    void mark_immortal()
    {
      loc = loc::Immortal;
    }

  public:
    uint32_t get_type_id()
    {
      return type_id;
    }

    RC get_rc()
    {
      return rc;
    }

    template<bool is_register = false>
    void dec()
    {
      if (dec_no_dealloc<is_register>())
        // Queue object/array for deallocation
        collect(this);
    }

    Location location() const
    {
      return loc;
    }

    Region* region()
    {
      if (!loc::is_region(loc))
        return nullptr;

      return loc::to_region(loc);
    }

    void move_region(Region* to)
    {
      if (loc::is_region(loc))
        loc::to_region(loc)->remove(this);

      loc = Location(to);
      to->insert(this);
    }

    bool sendable()
    {
      if (loc::is_immutable(loc))
      {
        return true;
      }
      else if (loc::is_stack(loc))
      {
        return false;
      }
      else
      {
        assert(loc::is_region(loc));
        auto r = loc::to_region(loc);

        if (r->sendable())
          return true;

        if (r->is_frame_local())
        {
          // Drag a frame-local allocation to a region.
          auto nr = Region::create(RegionType::RegionRC);

          if (!drag_allocation(nr, this))
            return false;

          if (nr->sendable())
            return true;

          // TODO: delay if stack_rc > 1?
        }

        return false;
      }
    }

    template<bool reg = false>
    void inc()
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

    void stack_inc()
    {
      if (loc::is_region(loc))
      {
        auto r = loc::to_region(loc);
        r->stack_inc();
      }
    }

    void stack_dec()
    {
      if (loc::is_region(loc))
      {
        auto r = loc::to_region(loc);
        r->stack_dec();
      }
    }
  };
}
