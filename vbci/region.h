#pragma once

#include "classes.h"
#include "collect.h"
#include "ident.h"
#include "location.h"

#include <iostream>
#include <vbci.h>
#include "logging.h"

namespace vbci
{
  struct Region
  {
  private:
    Location parent;
    RC stack_rc;

  protected:
    Region() : parent(Location::none()), stack_rc(0) {}
    virtual ~Region() = default;

  public:
    static Region* create(RegionType type);

    virtual Object* object(Class& cls) = 0;
    virtual Array* array(uint32_t type_id, size_t size) = 0;
    virtual void rfree(Header* h) = 0;
    virtual void insert(Header* h) = 0;
    virtual void remove(Header* h) = 0;
    virtual bool enable_rc() = 0;

    void stack_inc(RC inc = 1)
    {
      // TODO Should we restrict to only calling on non-frame-local regions?
      //      assert(!is_frame_local());

      // If we transition from 0 to 1, we need to increment the parent RC.
      if ((stack_rc == 0) && parent.is_region())
        parent.to_region()->stack_inc();

      LOG(Trace) << "Region @" << this << " stack_rc incremented from "
                << stack_rc << " to " << (stack_rc + inc);

      stack_rc += inc;
    }

    bool stack_dec()
    {
      // TODO Should we restrict to only calling on non-frame-local regions?
      // assert(!is_frame_local());

      LOG(Trace) << "Region @" << this << " stack_rc decremented from "
                << stack_rc << " to " << (stack_rc - 1);
      if (--stack_rc == 0)
      {
        if (!has_parent())
        {
          // Returns false if the region has been freed.
          free_region();
          return false;
        }

        // If we transition from 1 to 0, we need to decrement the parent RC.
        if (parent.is_region())
          return parent.to_region()->stack_dec();
      }

      return true;
    }

    bool is_ancestor_of(Region* r)
    {
      while (r->parent.is_region())
      {
        r = r->parent.to_region();

        if (r == this)
          return true;
      }

      return false;
    }

    bool sendable()
    {
      assert(stack_rc > 0);
      return !has_parent() && (stack_rc == 1);
    }

    bool has_parent()
    {
      return parent != Location::none();
    }

    Location get_parent()
    {
      return parent;
    }

    bool is_frame_local()
    {
      return parent.is_stack();
    }

    void set_frame_id(Location frame_id)
    {
      assert(!has_parent());
      assert(stack_rc > 0);
      parent = frame_id;
    }

    void set_parent(Region* r)
    {
      assert(!has_parent());
      parent = Location(r);

      if (stack_rc > 0)
        r->stack_inc();
    }

    void set_parent()
    {
      assert(!has_parent());
      parent = Location::immutable();
    }

    /**
     * Clear the parent of this region.
     *
     * If the region's stack RC is zero, returns true, indicating that the
     * region should be freed. Otherwise, returns false.
     */
    bool clear_parent()
    {
      assert(has_parent());

      if (parent.is_region() && (stack_rc > 0))
        parent.to_region()->stack_dec();

      parent = Location::none();
      // TODO: Should this just deallocate the region directly?
      return stack_rc == 0;
    }

    void free_region()
    {
      assert(!has_parent());
      assert(stack_rc == 0);
      collect(this);
    }

    /** Deallocate the region and its contents.
     *
     * This should not be called directly, but rather call free_region(), which
     * will handle issues with reentrancy.
     */
    void deallocate()
    {
      free_contents();
      delete this;
    }

  private:
    virtual void free_contents() = 0;
  };
}
