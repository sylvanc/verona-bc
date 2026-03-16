#pragma once

#include "classes.h"
#include "collect.h"
#include "ident.h"
#include "location.h"
#include "logging.h"

#include <cstdint>
#include <iostream>
#include <vector>
#include <vbci.h>

namespace vbci
{
  struct Header;

  struct Region
  {
  private:
    Region* parent;
    Header* entry_point;
    RC stack_rc;
    RegionType type;
    bool cown_owned;

    /**
     * Frame-local depth: 0 = not frame-local (heap region),
     * 1+ = frame depth (1 = outermost frame). Higher depth = younger frame.
     */
    size_t frame_depth;

  protected:
    Region(RegionType type, size_t frame_depth = 0)
    : parent(nullptr), entry_point(nullptr), stack_rc(0), type(type), cown_owned(false), frame_depth(frame_depth)
    {}

  public:
    virtual ~Region() = default;
    static Region* create(RegionType type, size_t frame_depth = 0);

    virtual Object* object(Class& cls) = 0;
    virtual Array* array(uint32_t type_id, size_t size) = 0;
    virtual void rfree(Header* h) = 0;
    virtual void insert(Header* h) = 0;
    virtual void remove(Header* h) = 0;
    virtual bool is_finalizing() = 0;
    virtual void finalize_contents() = 0;
    virtual void release_dead_objects() = 0;
    void for_each_header(auto&& fn) const;

    void trace_fn(auto&& fn) const;

    void stack_inc(RC inc = 1)
    {
      if (is_frame_local())
        return;

      // If we transition from 0 to 1, we need to increment the parent RC.
      if (stack_rc == 0)
      {
        if (has_parent())
        {
          LOG(Trace) << "Region @" << this
                     << " incrementing parent region stack_rc due to stack_rc "
                        "transitioning from 0 to 1";
          get_parent()->stack_inc();
        }
      }

      LOG(Trace) << "Region @" << this << " stack_rc incremented from "
                 << stack_rc << " to " << (stack_rc + inc);

      stack_rc += inc;
    }

    bool stack_dec(RC dec = 1)
    {
      if (is_frame_local())
        return true;

      assert(stack_rc >= dec);
      LOG(Trace) << "Region @" << this << " stack_rc decremented from "
                 << stack_rc << " to " << (stack_rc - dec);

      stack_rc -= dec;

      if (stack_rc == 0)
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
          LOG(Trace) << "Region @" << this
                     << " decrementing parent region stack_rc due to stack_rc "
                        "reaching 0";
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

    bool is_frame_local() const
    {
      return frame_depth > 0;
    }

    size_t get_frame_depth() const
    {
      return frame_depth;
    }

    bool has_owner() const
    {
      return parent || cown_owned;
    }

    bool has_parent() const
    {
      return parent != nullptr;
    }

    Region* get_parent() const
    {
      assert(parent);
      return parent;
    }

    void set_parent(Region* r, Header* entry)
    {
      LOG(Trace) << "Region @" << this << " setting parent region to @" << r;
      assert(!has_owner());
      assert(entry);
      parent = r;
      entry_point = entry;

      if (stack_rc > 0)
      {
        LOG(Trace)
          << "Region @" << this
          << " incrementing parent region stack_rc due to existing stack_rc";
        r->stack_inc();
      }
    }
    
    void clear_parent()
    {
      auto p = parent;
      assert(p != nullptr);
      parent = nullptr;
      entry_point = nullptr;

      if (stack_rc > 0)
        p->stack_dec();
      else
        free_region();
    }

    bool has_cown_owner() const
    {
      return cown_owned;
    }

    void set_cown_owner(Header* entry)
    {
      assert(!has_owner());
      assert(entry);
      cown_owned = true;
      entry_point = entry;
    }

    void clear_cown_owner()
    {
      assert(cown_owned);
      cown_owned = false;
      entry_point = nullptr;
    }

    Header* get_entry_point() const
    {
      return entry_point;
    }

    void free_region()
    {
      assert(!has_owner());
      assert(stack_rc == 0);
      collect(this);
    }

#ifndef NDEBUG
    RC get_stack_rc() const
    {
      return stack_rc;
    }
#endif
  };
}
