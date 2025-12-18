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
     * `parent` encodes the ownership of this region:
     *  - `nullptr` means the region currently has no owner.
     *  - `cown_region()` identifies that the region is owned by a Cown. This
     *    is a sentinel value used when a region is captured by a behaviour.
     *  - Any other pointer represents the real parent region. In this case
     *    stack reference counts are propagated along the parent chain so that
     *    dropping the child when its stack RC reaches zero will either free it
     *    or transfer the decrement to the parent.
     *
     * Consumers should prefer the helper accessors:
     *  - `has_owner()` to test whether the field is non-null.
     *  - `get_parent()` / `has_parent()` to work with only real (non-cown)
     *    parents.
     *  - `set_cown_owner()` / `clear_cown_owner()` to transition the sentinel
     *    state used while a Cown owns the region.
     */
    Region* parent;
    RC stack_rc;

    static Region* cown_region();


  protected:
    Region() : parent(nullptr), stack_rc(0) {}
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
      if (stack_rc == 0)
      {
        if (auto p = get_parent())
          p->stack_inc();
      }

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
        if (!has_owner())
        {
          // Returns false if the region has been freed.
          free_region();
          return false;
        }

        // If we transition from 1 to 0, we need to decrement the parent RC.
        if (auto p = get_parent())
          return p->stack_dec();
      }

      return true;
    }

    bool is_ancestor_of(Region* r)
    {
      while (auto p = r->get_parent())
      {
        r = p;

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
      return parent != nullptr;
    }

    Region* get_parent() const
    {
      return (parent && (parent != cown_region())) ? parent : nullptr;
    }

    bool has_parent() const
    {
      return get_parent() != nullptr;
    }

    Region* get_parent()
    {
      return const_cast<Region*>(
        static_cast<const Region*>(this)->get_parent());
    }

    bool is_frame_local();

    void set_parent(Region* r)
    {
      assert(!has_owner());
      parent = r;

      if (stack_rc > 0)
        r->stack_inc();
    }

    void set_cown_owner()
    {
      assert(!has_owner());
      parent = cown_region();
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

      parent = nullptr;
      // TODO: Should this just deallocate the region directly?
      return stack_rc == 0;
    }

    bool clear_cown_owner()
    {
      assert(parent == cown_region());
      parent = nullptr;
      // TODO: Should this just deallocate the region directly?
      return stack_rc == 0;
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
