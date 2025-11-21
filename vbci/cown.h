#pragma once

#include "header.h"
#include "program.h"
#include "region.h"
#include "thread.h"
#include "value.h"

#include <format>
#include <verona.h>

namespace vbci
{
  struct Cown : public verona::rt::VCown<Cown>
  {
  private:
    uint32_t type_id;
    Value content;

    Cown(uint32_t type_id) : type_id(type_id)
    {
      if (!Program::get().is_cown(type_id))
        throw Value(Error::BadType);
    }

  public:
    static Cown* create(uint32_t type_id)
    {
      auto cown = new Cown(type_id);
      LOG(Trace) << "Created cown @" << cown;
      return cown;
    }

    ~Cown()
    {
      LOG(Trace) << "Destroying cown @" << this;
      auto prev_loc = content.location();
      content.field_drop();
      if (loc::is_region(prev_loc) && loc::to_region(prev_loc)->clear_parent())
      {
        LOG(Trace) << "Freeing region: " << loc::to_region(prev_loc)
                   << " from cown " << this;
        loc::to_region(prev_loc)->free_region();
      }
      LOG(Trace) << "Destroyed cown @" << this;
    }

    uint32_t get_type_id()
    {
      return type_id;
    }

    uint32_t content_type_id()
    {
      return Program::get().uncown(type_id);
    }

    void inc()
    {
      LOG(Trace) << "Incrementing cown @" << this;
      acquire(this);
    }

    void dec()
    {
      LOG(Trace) << "Decrementing cown @" << this;
      release(this);
    }

    Value load()
    {
      return content.copy();
    }

    Value store(bool move, Value& next)
    {
      bool bad_store = false;
      bool unparent_prev = true;
      Region* nr;

      // Allow any cown to contain an error.
      if (
        !next.is_error() &&
        !Program::get().subtype(next.type_id(), content_type_id()))
        next = Value(Error::BadType);

      auto prev_loc = content.location();
      auto next_loc = next.location();

      // Can't store a stack value in a cown.
      if (loc::is_stack(next_loc))
      {
        bad_store = true;
      }
      else if (loc::is_region(next_loc) && (next_loc != prev_loc))
      {
        // It doesn't matter what the stack RC is, because all stack RC will be
        // gone by the time this cown is available to any other behavior.
        auto r = loc::to_region(next_loc);

        if (r->is_frame_local())
        {
          LOG(Trace) << "Dragging frame-local allocation to new region:" << r;
          // Drag a frame-local allocation to a fresh region.
          nr = Region::create(RegionType::RegionRC);
          nr->set_parent();

          // if ploc is a region, must be not frame local
          if (loc::is_region(prev_loc))
          {
            loc::to_region(prev_loc)->clear_parent();
            unparent_prev = false;
          }

          if (!drag_allocation(nr, next.get_header()))
          {
            bad_store = true;
            nr->free_region();
          }
          else
          {
            next_loc = Location(nr);
            // Remove the stack reference to this region, as we have moved it
            // into the cown.
            if (move)
              r->stack_dec();
          }
        }
        else if (r->has_parent())
        {
          // If the region has a parent, it can't be stored.
          bad_store = true;
        }
        else
        {
          LOG(Trace) << "Adding region: " << r << " to cown " << this;
          // Set the region parent to this cown.
          r->set_parent();
          // Remove the stack reference to this region, as we have moved it into
          // the cown.
          if (move)
            r->stack_dec();
        }
      }

      Value prev = std::move(content);
      if (bad_store)
      {
        LOG(Error) << next.to_string();
        content = Value(Error::BadStore);
      }
      else
      {
        if (move)
          content = std::move(next);
        else
          content = next.copy();
      }

      // Clear prev region parent if it's different from next.
      if (loc::is_region(prev_loc) && (prev_loc != next_loc))
      {
        if (unparent_prev)
        {
          LOG(Trace) << "Removing region: " << loc::to_region(prev_loc)
                     << " from cown " << this;
          loc::to_region(prev_loc)->clear_parent();
        }
        loc::to_region(prev_loc)->stack_inc();
      }
      return prev;
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };
}
