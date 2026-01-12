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

    // clang-format off
    /** 
     * Add region reference  (ignoring classic RC operations for now)
     *
     *  | Location of | Location of Target                                                             |
     *  | Source      | Stack          | Frame Local           | Region                    | Immutable |
     *  |-------------|----------------|-----------------------|---------------------------|-----------|
     *  | Stack       | Check Lifetime | Check lifetime, drag? | Stack inc (if not move)   |  Nop      |
     *  | Frame Local | Not allowed    | Check lifetime, drag? | Stack inc (if not move)   |  Nop      |
     *  | Region      | Not allowed    | Drag                  | Parent (stack dec if move)|  Nop      |
     * 
     * This returns false if the region reference cannot be added.
     * It updates the stack RCs and parents as needed.
     **/
    // clang-format on
    template<bool is_move>
    bool add_region_reference(const Value& next) const
    {
      constexpr bool is_copy = !is_move;
      auto nloc = next.location();
      auto cloc = loc;

      if (nloc.is_immutable())
        return true;

      if (nloc.is_stack())
      {
        if (!cloc.is_stack())
          return false;

        // nloc must outlive loc.
        return cloc.stack_index() >= nloc.stack_index();
      }

      if (nloc.is_frame_local())
      {
        if (cloc.is_stack())
        {
          // Check for outliving,
          if (cloc.stack_index() >= nloc.frame_local_index())
            return true;
          // Drag if nloc doesn't outlive loc.
          cloc = Location::frame_local(cloc.stack_index());
        }
        else if (
          cloc.is_frame_local() &&
          cloc.frame_local_index() >= nloc.frame_local_index())
        {
          // nloc must outlive loc.
          // otherwise drag.
          return true;
        }

        return drag_allocation<is_move>(cloc, next.get_header());
      }

      // TODO still needed
      if (nloc.no_rc())
        return true;

      assert(nloc.is_region());

      if (cloc.is_stack() || cloc.is_frame_local())
      {
        auto nr = nloc.to_region();

        // For copy, need to increase stack RC.
        if constexpr (is_copy)
          nr->stack_inc();
        return true;
      }

      auto r = cloc.to_region();
      auto nr = nloc.to_region();

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

      assert(cloc.is_region());

      // Set the parent
      // it's in a different region.
      nr->set_parent(r);

      // For the move case, decrement the region stack RC. This can't free the
      // region as we just parented it.
      if constexpr (is_move)
        nr->stack_dec();
      return true;
    }

    // clang-format off
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
     * 
     * | Location of | Location of old Target                                                                       |
     * | Source      | Stack | Frame Local | Region                                                     | Immutable |
     * |-------------|-------|-------------|------------------------------------------------------------|-----------|
     * | Stack       |  Nop  | Nop         | Nop  (stack dec if dead)                                   |   Nop     |
     * | Frame Local |  NA   | Nop         | Nop  (stack dec if dead)                                   |   Nop     |
     * | Region      |  NA   | NA          | Unparent + (stack inc if live) unless same region, then Nop|   Nop     |
     */
    // clang-format on
    template<bool to_register = true>
    bool remove_region_reference(Location ploc) const
    {
      if (loc.is_frame_local() || loc.is_stack())
      {
        // This is being removed from an object/array that is frame local, and
        // will not land in register we need to remove its stack rc.
        if constexpr (!to_register)
          return ploc.to_region()->stack_dec();
        return true;
      }

      if (!ploc.is_region())
        return true;

      auto pr = ploc.to_region();

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
      if (!ploc.is_region())
        return;

      auto pr = ploc.to_region();
      auto r = loc.to_region();

      if (loc.is_frame_local() || loc.is_stack())
      {
        return;
      }

      // We need to reverse the order with respect to remove_region_reference
      // So reparent if we unparented, and then remove the stack inc.
      if (pr != r)
        pr->set_parent(r);

      pr->stack_dec();
    }

    template<bool is_move, bool no_dst = false>
    void exchange(Register* dst, void* addr, ValueType t, Reg<is_move> next) const
    {
      assert(no_dst == (dst == nullptr));
      
      if (loc.is_immutable())
        Value::error(Error::BadStoreTarget);

      auto nloc = next->location();

      Value prev;
      if constexpr (!no_dst)
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
          next->to_addr(t, addr);
          // We have overwritten the previous value, so the classic RC invariant
          // is reestablished.

          if constexpr (!is_move)
          {
            // We need another RC for the value stored in the reference.
            // We also need a stack RC if it is a region as prev is going into a
            // register and that has the same region.
            // Hence we need:
            //   prev.stack_inc();
            //   next.inc<false>();
            // as prev and next are the same location we can optimise this to:
            next->template inc<true>();
          }
          else
          {
            // We have consumed the register, so clear.
            // We have taken ownership into the written location.
            next.clear_unsafe();
          }

          *dst = ValueTransfer(prev);
          return;
        }

        remove_region_reference(ploc);
        // The stack RC invariant for prev now holds, but it is still
        // missing a classic RC.  Overwriting the value will establish this,
        // but in the case of failure we need to invalidate prev to avoid
        // it removing a classic RC it was never entitled to.
      }

      if (!add_region_reference<is_move>(next.borrow()))
      {
        // Failed to add references, need to restore prev's RCs.
        if constexpr (!no_dst)
        {
          auto ploc = prev.location();
          restore_reference(ploc);
        }

        Value::error(Error::BadStore);
      }

      if constexpr (!is_move)
      {
        // TODO should add_region_reference have already done this?
        next->template inc<false>();
      }
      // Now safe to perform the write.
      next->to_addr(t, addr);

      if constexpr (is_move)
      {
      // We have consumed the register, so clear.
        // We have taken ownership into the written location.
        next.clear_unsafe();
      }

      // We have overwritten the previous value, so the classic RC invariant is
      // reestablished for prev.
      if constexpr (!no_dst)
        *dst = ValueTransfer(prev);
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
    template<bool needs_stack_rc>
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

      if (!needs_stack_rc)
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

    template<bool needs_stack_rc>
    void dec()
    {
      if (dec_no_dealloc<needs_stack_rc>())
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
      assert(to_loc.is_region_or_frame_local());

      loc.to_region()->remove(this);
      loc = to_loc;
      to->insert(this);
    }

    bool sendable()
    {
      if (loc.is_stack())
      {
        return false;
      }
      else if (loc.is_frame_local())
      {
        // Drag a frame-local allocation to a region.
        auto nr = Region::create(RegionType::RegionRC);

        // TODO: review is this a move or copy for the template parameter?
        if (!drag_allocation<false>(Location(nr), this))
          return false;

        return (nr->sendable());

        // TODO: delay if stack_rc > 1?
      }
      else if (loc.is_region())
      {
        auto r = loc.to_region();
        if (r->sendable())
          return true;

        return false;
      }
      return true;
    }

    template<bool needs_stack_rc>
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

      // If this RC inc comes from a register or frame local object,
      // increment the region stack RC.
      if (needs_stack_rc)
        r->stack_inc();

      if (r->enable_rc())
        rc++;
    }

    void stack_inc()
    {
      if (loc.is_region())
      {
        auto r = loc.to_region();
        r->stack_inc();
      }
    }

    void stack_dec()
    {
      if (loc.is_region())
      {
        auto r = loc.to_region();
        r->stack_dec();
      }
    }
  };
}
