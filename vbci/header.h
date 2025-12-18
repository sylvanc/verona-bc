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

      if (nloc.is_immutable())
        return true;

      if (loc.is_stack())
      {
        if (nloc.is_stack())
          // Older frames can't point to newer frames.
          return loc.stack_index() >= nloc.stack_index();

        assert(nloc.is_region_or_frame_local());

        if (nloc.no_rc())
          return true;

        auto nr = nloc.to_region();

        // Older frames can't point to newer frames.
        if (nloc.is_frame_local())
        {
          if (loc.stack_index() < nloc.frame_local_index())
            return false;

          // Stack to frame local doesn't need a stack rc.
          return true;
        }

        if constexpr (is_copy)
          // Copy so need to increase stack RC.
          nr->stack_inc();
        return true;
      }

      assert(loc.is_region_or_frame_local());

      if (nloc.is_stack())
        // No region, even a frame-local one, can point to the stack.
        return false;

      assert(nloc.is_region_or_frame_local());

      if (nloc.no_rc())
        return true;

      auto r = loc.to_region();
      auto nr = nloc.to_region();

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
        auto success = drag_allocation(loc, next.get_header());
        if (!success)
          return false;
        // TODO Should perhaps move this inside drag_allocation?
        if (is_move && !r->is_frame_local())
          r->stack_dec();
        return true;
      }

      if (nr->has_owner())
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
      assert(!loc.is_stack());

      assert(nloc.is_region_or_frame_local());

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
      if (!ploc.is_region() || ploc.no_rc())
        return true;

      assert(!ploc.is_stack());

      auto pr = ploc.to_region();
      auto r = loc.to_region();

      // Check if the region is frame-local and hence does not need unparenting.
      if (pr->is_frame_local())
        // We don't need stack rc for frame_local regions either.
        return true;

      if (r->is_frame_local())  // TODO are we missing a stack case here?
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
      if (ploc == loc)
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
      if (!ploc.is_region() || ploc.no_rc())
        return;

      auto pr = ploc.to_region();
      auto r = loc.to_region();

      if (pr->is_frame_local())
        return;

      if (r->is_frame_local())
      {
        return;
      }

      // We need to reverse the order with respect to remove_region_reference
      // So reparent if we unparented, and then remove the stack inc.
      if (pr != r)
        r->set_parent(loc.to_region());

      pr->stack_dec();
    }

    template<bool is_move, bool no_previous = false>
    Register store(void* addr, ValueType t, Reg<is_move> next) const
    {
      if (loc.is_immutable())
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

          if (ploc.is_region() && !ploc.is_frame_local() && !ploc.no_rc())
          {
            if constexpr (!is_move)
              ploc.to_region()->stack_inc();
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
      assert(loc.is_immutable());
      auto c = this;
      auto p = loc.to_scc();

      if (!p)
        return c;

      while (true)
      {
        auto gp = p->loc.to_scc();

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
      if (loc.no_rc())
        return false;

      if (loc.is_immutable())
      {
        // TODO: how do we correctly free an SCC?
        return --get_scc()->arc == 0;
      }

      auto r = loc.to_region();
      bool ret = false;

      if (r->enable_rc())
        ret = --rc == 0;

      if (!is_register)
        return ret;

      return r->stack_dec() && ret;
    }

    void mark_immortal()
    {
      loc = Location::immortal();
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
      if (!loc.is_region_or_frame_local() || loc.no_rc())
        return nullptr;

      return loc.to_region();
    }

    void move_region(Location to_loc, Region* to)
    {
      if (loc.is_region_or_frame_local())
        loc.to_region()->remove(this);

      assert(to_loc.is_region_or_frame_local());
      loc = to_loc;
      to->insert(this);
    }

    bool sendable()
    {
      if (loc.is_immutable())
      {
        return true;
      }
      else if (loc.is_stack())
      {
        return false;
      }
      else if (loc.no_rc())
      {
        // Immortal values do not participate in region reference counting.
        return true;
      }
      else
      {
        assert(loc.is_region_or_frame_local());
        auto r = loc.to_region();

        if (r->sendable())
          return true;

        if (r->is_frame_local())
        {
          // Drag a frame-local allocation to a region.
          auto nr = Region::create(RegionType::RegionRC);

          if (!drag_allocation(Location(nr), this))
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
      if (loc.no_rc())
        return;

      if (loc.is_immutable())
      {
        get_scc()->arc++;
        return;
      }

      auto r = loc.to_region();

      // If this RC inc comes from a register, increment the region stack RC.
      if (reg)
        r->stack_inc();

      if (r->enable_rc())
        rc++;
    }

    void stack_inc()
    {
      if (loc.is_region_or_frame_local())
      {
        auto r = loc.to_region();
        r->stack_inc();
      }
    }

    void stack_dec()
    {
      if (loc.is_region_or_frame_local())
      {
        auto r = loc.to_region();
        r->stack_dec();
      }
    }
  };
}
