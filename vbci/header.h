#pragma once

#include "ident.h"
#include "location.h"
#include "logging.h"
#include "region.h"
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

    bool add_reference(const Value& next)
    {
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
        if (nr->is_frame_local() && (loc < nr->get_parent()))
          return false;

        return true;
      }

      assert(loc::is_region(loc));

      if (loc::is_stack(nloc))
        // No region, even a frame-local one, can point to the stack.
        return false;

      assert(loc::is_region(nloc));
      auto r = loc::to_region(loc);
      auto nr = loc::to_region(nloc);

      // Creating an interior region reference is fine and has no accounting.
      if (r == nr)
      {
        // We need to remove the stack rc from the passed in value.
        nr->stack_dec();
        return true;
      }

      if (nr->is_frame_local())
      {
        // Drag a frame-local allocation to a region.
        return drag_allocation(r, next.get_header());
      }

      if (r->is_frame_local())
        return true;

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

      // Decrement the region stack RC. This can't free the region as we just
      // parented it.
      nr->stack_dec();
      return true;
    }

    /**
     * @brief Remove a reference from the region meta-data.
     * 
     *
     * It assumes the `prev` value will be stored into a register, so
     * increments the stack reference count if the location is a region,
     * and the underlying pointer didn't already come from a stack or
     * frame-local region.
     * 
     * to_register: Indicates whether the value is going into a register.
     * This will then perform the required stack RC increment.
     */
    template <bool to_register = true>
    void remove_reference(const Value& prev)
    {
      auto ploc = prev.location();

      if (!loc::is_region(ploc))
        return;

      assert(!loc::is_stack(ploc));

      auto pr = loc::to_region(ploc);
      auto r = loc::to_region(loc);

      // Internal edges have no accounting.
      if (pr == r) {
        // As this is going into a register, increment the stack RC.
        if (to_register)
          pr->stack_inc();
        return;
      }

      // Check if the region is frame-local and hence does not need unparenting.
      if (pr->is_frame_local())
        // This location already has a stack reference count, so no action is
        // needed.
        return;

      // This is being removed from an object/array, and will land in a
      // register, so we need to increment the stack RC to account for that.
      if constexpr (to_register)
        pr->stack_inc();

      bool cleared = pr->clear_parent();
      if constexpr (!to_register)
        if (cleared)
          pr->free_region();
    }

    // This is used when the write barrier fails, and we need to undo the
    // effects of `remove_reference`.
    template <bool to_register = true>
    void restore_reference(const Value& prev)
    {
      auto ploc = prev.location();

      if (!loc::is_region(ploc))
        return;

      auto pr = loc::to_region(ploc);
      auto r = loc::to_region(loc);

      if (pr == r)
      {
        if (to_register)
          pr->stack_dec();
        return;
      }

      if (r->is_frame_local())
        return;

      r->set_parent(loc::to_region(loc));

      if (to_register)
        r->stack_dec();
    }

    bool write_barrier(Value& prev, Value& next)
    {
      auto ploc = prev.location();
      auto nloc = next.location();

      if (loc::is_immutable(loc))
        // Nothing can be stored to an immutable location.
        return false;

      // If the previous and next locations are the same, no action is needed.
      if (ploc == nloc)
        return true;

      // It is safe to first remove the previous reference, as that can't
      // free the region being written to because we have set `to_register` to true,
      // which increments the stack RC.
      remove_reference<true>(prev);

      if (add_reference(next))
        return true;

      // Adding the reference failed, so we need to restore the previous state.
      restore_reference<true>(prev);
      return false;
    }

    void field_drop(Value& prev)
    {
      remove_reference<false>(prev);
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

    template <bool reg>
    bool base_dec()
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
    uint32_t get_type_id()
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

    template <bool reg>
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
  };
}
