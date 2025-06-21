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

    TypeId type_id;

  protected:
    Header(Location loc, TypeId type_id) : loc(loc), rc(1), type_id(type_id) {}

    bool write_barrier(Value& prev, Value& next)
    {
      auto ploc = prev.location();
      auto nloc = next.location();

      if (loc::is_immutable(loc))
      {
        // Nothing can be stored to an immutable location.
        return false;
      }
      else if (loc::is_stack(loc))
      {
        if (loc::is_immutable(nloc))
        {
          // Ok.
        }
        else if (loc::is_stack(nloc))
        {
          // Older frames can't point to newer frames.
          if (loc < nloc)
            return false;
        }
        else
        {
          assert(loc::is_region(nloc));
          auto nr = loc::to_region(nloc);

          // Older frames can't point to newer frames.
          if (nr->is_frame_local() && (loc < nr->get_parent()))
            return false;
        }
      }
      else
      {
        assert(loc::is_region(loc));

        if (loc::is_immutable(nloc))
        {
          // Ok.
        }
        else if (loc::is_stack(nloc))
        {
          // No region, even a frame-local one, can point to the stack.
          return false;
        }
        else
        {
          assert(loc::is_region(nloc));
          auto r = loc::to_region(loc);
          auto nr = loc::to_region(nloc);

          if (r == nr)
          {
            // Ok.
          }
          else if (nr->is_frame_local())
          {
            // Drag a frame-local allocation to a region.
            if (!drag_allocation(r, next.get_header()))
              return false;
          }
          else if (nr->has_parent())
          {
            // Regions can only have a single entry point.
            if (!loc::is_region(ploc) || (loc::to_region(ploc) != nr))
              return false;
          }
          else if (nr->is_ancestor_of(r))
          {
            // Regions can't form cycles.
            return false;
          }
        }
      }

      // At this point, the write barrier is satisfied.
      if (
        (ploc == nloc) || loc::is_stack(loc) ||
        loc::to_region(loc)->is_frame_local())
      {
        // If the previous and next locations are the same, or if the location
        // is a stack allocation, or if the location is a frame-local region, no
        // action is needed.
        return true;
      }

      if (loc::is_region(ploc))
      {
        // Increment the region stack RC.
        auto pr = loc::to_region(ploc);
        pr->stack_inc();

        // Clear the parent if it's in a different region.
        if (ploc != loc)
          pr->clear_parent();
      }

      if (loc::is_region(nloc))
      {
        auto nr = loc::to_region(nloc);

        // Set the parent if it's in a different region.
        if (nloc != loc)
          nr->set_parent(loc::to_region(loc));

        // Decrement the region stack RC. This can't free the region.
        nr->stack_dec();
      }

      return true;
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
    TypeId get_type_id()
    {
      return type_id;
    }

    RC get_rc()
    {
      return rc;
    }

    bool is_array()
    {
      return type_id.is_array();
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

    void move_region(Region* to)
    {
      if (loc::is_region(loc))
        loc::to_region(loc)->remove(this);

      loc = Location(to);
      to->insert(this);
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
