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

    Register load()
    {
      return content.copy_reg();
    }

    template<bool is_move>
    bool add_region_reference(Reg<is_move>& next)
    {
      auto next_loc = next.location();
      // Can't store a stack value in a cown.
      if (loc::is_stack(next_loc))
        return false;

      // Primitives and immutables are always ok.
      if (!loc::is_region(next_loc))
        return true;

      // It doesn't matter what the stack RC is, because all stack RC will be
      // gone by the time this cown is available to any other behavior.
      auto r = loc::to_region(next_loc);

      if (r->is_frame_local())
      {
        // Drag a frame-local allocation to a fresh region.
        r = Region::create(RegionType::RegionRC);
        LOG(Trace) << "Dragging frame-local allocation to new region:" << r;

        if (!drag_allocation(r, next.get_header()))
        {
          r->free_region();
          return false;
        }
      }

      if (r->has_parent())
        // If the region has a parent, it can't be stored.
        return false;

      r->set_parent();
      // Remove the stack reference to this region, as we have moved it
      // into the cown.
      if constexpr (is_move)
        r->stack_dec();
      return true;
    }

    template<bool is_move>
    Register store(Reg<is_move> next)
    {
      if (
        !next.is_error() &&
        !Program::get().subtype(next.type_id(), content_type_id()))
        throw Value(Error::BadType);

      auto prev_loc = content.location();
      auto next_loc = next.location();

      // If in the same location/region, we can be simple.
      if (next_loc == prev_loc)
      {
        Value prev = std::move(content);

        if constexpr (is_move)
        {
          content = std::move(next);
        }
        else
        {
          content = next.copy_value();
          // If we copied, need to increment stack RC as there is a new
          // Register reference to this region.
          if (loc::is_region(next_loc))
            loc::to_region(next_loc)->stack_inc();
        }
        return Register(std::move(prev));
      }

      // if ploc is a region, must be not frame local
      if (loc::is_region(prev_loc))
      {
        auto pr = loc::to_region(prev_loc);
        LOG(Trace) << "Removing region: " << pr << " from cown " << this;
        assert(!pr->is_frame_local());
        // Previous value will land in a register, so increment stack RC.
        pr->stack_inc();
        // Need to clear parent before drag, incase the drag will
        // reparent this region.
        pr->clear_parent();
      }

      if (!add_region_reference<is_move>(next))
      {
        // Failed to add references, need to restore the previous contents.
        if (loc::is_region(prev_loc))
        {
          auto pr = loc::to_region(prev_loc);
          LOG(Trace) << "Restoring region: " << pr << " to cown " << this;
          pr->set_parent();
          pr->stack_dec();
        }
        throw Value(Error::BadStore);
      }

      // The code above has ensured we have suitable stack rc if required.
      Register prev{std::move(content)};

      if constexpr (is_move)
        content = std::move(next);
      else
        content = next.copy_value();

      return prev;
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };
}
