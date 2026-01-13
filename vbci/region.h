#pragma once

#include "classes.h"
#include "collect.h"
#include "ident.h"
#include "location.h"

#include <cstdint>
#include <iostream>
#include <vbci.h>
#include "logging.h"

namespace vbci
{
  struct Region
  {
  private:
    /**
     * `parent` encodes ownership information using the low two bits:
     *  - `0` means the region currently has no owner.
     *  - `tag == 0` and non-zero value stores the actual parent region pointer
     *    (which is guaranteed to be at least 4-byte aligned).
     *  - `tag == 1` marks that the region is owned by a cown while it is
     *    captured by a behaviour.
     *  - `tag == 2` marks that the region is frame-local and currently owned
     *    by the running thread.
     *
     * The helpers below should be preferred to reading or writing this field
     * directly.
     */
    uintptr_t parent;
    RC stack_rc;

    static constexpr uintptr_t parent_tag_mask = 0x3;
    static constexpr uintptr_t parent_tag_cown = 0x1;
    static constexpr uintptr_t parent_tag_frame_local = 0x2;


  protected:
    Region() : parent(0), stack_rc(0) {}
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
      if (has_frame_local_owner())
        return;

      // If we transition from 0 to 1, we need to increment the parent RC.
      if (stack_rc == 0)
      {
        if (has_parent()) {
          LOG(Trace) << "Region @" << this << " incrementing parent region stack_rc due to stack_rc transitioning from 0 to 1";
          get_parent()->stack_inc();
        }
      }

      LOG(Trace) << "Region @" << this << " stack_rc incremented from "
                << stack_rc << " to " << (stack_rc + inc);

      stack_rc += inc;
    }

    bool stack_dec()
    {
      if (has_frame_local_owner())
        return true;

      assert(stack_rc > 0);

      LOG(Trace) << "Region @" << this << " stack_rc decremented from "
                << stack_rc << " to " << (stack_rc - 1);
      if (--stack_rc == 0)
      {
        if (!has_owner())
        {
          // Returns false if the region has been freed.
          free_region();
          return false;
        }

        // If we transition from 1 to 0, we need to decrement the parent RC.
        if (has_parent())
        {
          LOG(Trace) << "Region @" << this << " decrementing parent region stack_rc due to stack_rc reaching 0";
          return get_parent()->stack_dec();
        }
      }

      return true;
    }

    bool is_ancestor_of(Region* r)
    {
      while (r->has_parent())
      {
        r = r->get_parent();

        if (r == this)
          return true;
      }

      return false;
    }

    bool sendable()
    {
      assert(stack_rc > 0);
      return !has_owner() && (stack_rc == 1);
    }

    bool has_owner() const
    {
      return (parent != 0);
    }

    Region* get_parent() const
    {
      assert(has_parent());
      return reinterpret_cast<Region*>(parent);
    }

    bool has_parent() const
    {
      return (parent & ~parent_tag_mask) != 0;
    }

#ifndef NDEBUG
    RC get_stack_rc() const
    {
      return stack_rc;
    }
#endif

    void set_parent(Region* r)
    {
      LOG(Trace) << "Region @" << this << " setting parent region to @" << r;
      assert(!has_owner());
      auto raw = reinterpret_cast<uintptr_t>(r);
      assert((raw & parent_tag_mask) == 0);
      parent = raw;

      if (stack_rc > 0) {
        LOG(Trace) << "Region @" << this << " incrementing parent region stack_rc due to existing stack_rc";
        r->stack_inc();
      }
    }

    void set_cown_owner()
    {
      assert(!has_owner());
      parent = parent_tag_cown;
    }

    void set_frame_local_owner()
    {
      assert(parent == 0);
      parent = parent_tag_frame_local;
    }

    /**
     * Clear the parent of this region.
     *
     * If the region's stack RC is zero, returns true, indicating that the
     * region should be freed. Otherwise, returns false.
     */
    bool clear_parent()
    {
      auto p = get_parent();
      assert(p != nullptr);

      if (stack_rc > 0)
        p->stack_dec();

      parent = 0;
      // TODO: Should this just deallocate the region directly?
      return stack_rc == 0;
    }

    bool clear_cown_owner()
    {
      assert(parent == parent_tag_cown);
      parent = 0;
      // TODO: Should this just deallocate the region directly?
      return stack_rc == 0;
    }

    bool has_cown_owner() const
    {
      return parent == parent_tag_cown;
    }

    bool has_frame_local_owner() const
    {
      return parent == parent_tag_frame_local;
    }

    void free_region()
    {
      assert(!has_owner());
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
