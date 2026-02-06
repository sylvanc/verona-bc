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
        Value::error(Error::BadType);
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
      Region* prev_region = nullptr;

      if (prev_loc.is_region())
      {
        prev_region = prev_loc.to_region();
        prev_region->stack_inc();
      }

      content.dec<false>();
      content = Value();

      if (prev_region != nullptr)
      {
        prev_region->clear_cown_owner();
        prev_region->stack_dec();
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

    ValueBorrow load()
    {
      return content;
    }

    template<bool is_move>
    bool add_reference(Reg<is_move>& next)
    {
      auto next_loc = next->location();
      // Can't store a stack value in a cown.
      if (next_loc.is_stack())
        return false;

      // Primitives and immutables are always ok.
      if (!next_loc.is_region_or_frame_local())
      {
        if constexpr (!is_move)
          next->template inc<false>();

        content = next.borrow();
        return true;
      }

      Region* r;

      if (next_loc.is_region())
      {
        r = next_loc.to_region();
        if (r->has_owner())
          // If the region has a parent, it can't be stored.
          return false;

        r->set_cown_owner();

        if constexpr (is_move)
        {
          r->stack_dec();
          // Remove the stack reference to this region, as we have moved it
          // into the cown.
          content = next.extract();
        }
        else
        {
          next->template inc<false>();
          content = next.borrow();
        }

        return true;
      }

      // It doesn't matter what the stack RC is, because all stack RC will be
      // gone by the time this cown is available to any other behavior.

      // Drag a frame-local allocation to a fresh region.
      r = Region::create(RegionType::RegionRC);
      LOG(Trace) << "Dragging frame-local allocation to new region:" << r;

      if (!drag_allocation<is_move>(Location(r), next->get_header()))
      {
        r->free_region();
        return false;
      }

      r->set_cown_owner();
      // Remove the stack reference to this region, as we have moved it
      // into the cown.
      if constexpr (is_move)
      {
        // TODO: I think this stack dec was performed by the drag?
        content = next.extract();
      }
      else
      {
        next->template inc<false>();
        content = next.borrow();
      }

      return true;
    }

    template<bool is_move, bool no_dst = false>
    void exchange(Register* dst, Reg<is_move> next)
    {
      if (
        !next->is_error() &&
        !Program::get().subtype(next->type_id(), content_type_id()))
        Value::error(Error::BadType);

      Value prev = content;
      auto prev_loc = prev.location();
      auto next_loc = next->location();

      // If in the same location/region, we can be simple.
      if (next_loc == prev_loc)
      {
        if constexpr (is_move)
        {
          content = next.extract();
          // We are effectively doing:
          //  prev.stack_inc();
          //  next.stack_dec();
          // But since they are the same region we can elide.
          // However, if there is no_dst then we do need to dec next.
          if constexpr (no_dst)
            content.stack_dec();
        }
        else
        {
          next->template inc<false>();
          content = next.borrow();


          if (!no_dst)
            prev.stack_inc();
        }

        if (!no_dst)
          *dst = ValueTransfer(prev);
        return;
      }

      if (!no_dst && prev_loc.is_region())
      {
        auto pr = prev_loc.to_region();
        LOG(Trace) << "Removing region: " << pr << " from cown " << this;
        // Previous value will land in a register, so increment stack RC.
        pr->stack_inc();
        // Need to clear parent before drag, in case the drag will
        // reparent this region.
        pr->clear_cown_owner();
      }

      if (!add_reference<is_move>(next))
      {
        // Failed to add references, need to reestablish region invariant for
        // previous.
        if (!no_dst && prev_loc.is_region())
        {
          auto pr = prev_loc.to_region();
          LOG(Trace) << "Restoring region: " << pr << " to cown " << this;
          pr->set_cown_owner();
          pr->stack_dec();
        }
        Value::error(Error::BadStore);
      }

      if constexpr (!no_dst)
        *dst = ValueTransfer(prev);
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };
}
