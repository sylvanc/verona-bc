#pragma once

#include "drag.h"
#include "header.h"
#include "program.h"
#include "region.h"
#include "value.h"

#include <atomic>
#include <boc/behaviourcore.h>
#include <ds/heap.h>
#include <format>
#include <new>

namespace vbci
{
  struct VbciObjectModel;

  struct alignas(8) Cown
  {
  private:
    friend VbciObjectModel;

    static constexpr size_t immortal_bit = 1;
    static constexpr size_t reference_increment = 2;

    union LifetimeState
    {
      std::atomic<size_t> references;
      Cown* next_to_delete;

      LifetimeState() : references(reference_increment) {}
      ~LifetimeState() {}
    };

    verona::rt::CownSchedulerState<VbciObjectModel> scheduler_state;
    LifetimeState lifetime;
    uint32_t type_id;
    Value content;

    Cown(uint32_t type_id) : type_id(type_id) {}

    static void destroy(Cown* cown) noexcept
    {
      thread_local Cown* pending = nullptr;
      thread_local bool draining = false;

      cown->lifetime.next_to_delete = pending;
      pending = cown;

      if (draining)
        return;

      draining = true;
      while (pending != nullptr)
      {
        auto* current = pending;
        pending = current->lifetime.next_to_delete;
        current->~Cown();
        verona::rt::heap::dealloc<sizeof(Cown)>(current);
      }
      draining = false;
    }

  public:
    static Cown* create(uint32_t type_id)
    {
      if (!Program::get().is_cown(type_id))
        Value::error(Error::BadType);

      auto* memory = verona::rt::heap::alloc<sizeof(Cown)>();
      auto* cown = new (memory) Cown(type_id);
      LOG(Trace) << "Created cown @" << cown;
      return cown;
    }

    ~Cown()
    {
      LOG(Trace) << "Destroying cown @" << this;
      auto loc = content.location();
      Region* r = nullptr;

      // Temporarily protect the region from premature free during field_dec.
      if (loc.is_region())
      {
        r = loc.to_region();
        r->stack_inc();
      }

      content.field_dec();
      content = Value();

      // Release cown ownership of the region.
      if (r)
      {
        r->clear_cown_owner();
        r->stack_dec();
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

    void inc() noexcept
    {
      LOG(Trace) << "Incrementing cown @" << this;
      assert(
        (lifetime.references.load(std::memory_order_relaxed) & ~immortal_bit) >
        0);
      lifetime.references.fetch_add(
        reference_increment, std::memory_order_relaxed);
    }

    void dec() noexcept
    {
      LOG(Trace) << "Decrementing cown @" << this;
      auto references = lifetime.references.fetch_sub(
        reference_increment, std::memory_order_release);
      assert((references & ~immortal_bit) > 0);
      if (references != reference_increment)
        return;

      std::atomic_thread_fence(std::memory_order_acquire);
      destroy(this);
    }

    void immortalize() noexcept
    {
      lifetime.references.fetch_or(immortal_bit, std::memory_order_relaxed);
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

      Region* r = nullptr;

      if (next_loc.is_region())
      {
        r = next_loc.to_region();

        if (r->is_frame_local())
        {
          // Frame-local: drag to a fresh region first.
          auto nr = Region::create(RegionType::RegionRC);
          LOG(Trace) << "Dragging frame-local allocation to new region: " << nr;

          if (!drag_allocation<is_move>(nr, next->get_header()))
          {
            nr->free_region();
            return false;
          }

          r = nr;
        }
        else if (r->has_owner())
        {
          return false;
        }
      }

      // Store the value.
      if constexpr (is_move)
        content = next.extract();
      else
      {
        next->field_inc();
        content = next.borrow();
      }

      // Set cown ownership on the region.
      if (r)
      {
        r->set_cown_owner(this, content.get_header());

        // For non-dragged regions, remove the register's stack reference —
        // the cown now owns the region. For frame-local drags, the drag
        // already adjusted the stack RC.
        if constexpr (is_move)
        {
          if (next_loc.is_region() && !next_loc.to_region()->is_frame_local())
            r->stack_dec();
        }
      }

      return true;
    }

    template<bool is_move>
    ValueTransfer exchange(Reg<is_move> next)
    {
      if (
        !next->is_error() &&
        !Program::get().subtype(next->type_id(), content_type_id()))
        Value::error(Error::BadType);

      Value prev = content;
      auto prev_loc = prev.location();
      auto next_loc = next->location();

      // Same location/region: simple swap, no region ownership changes.
      if (next_loc == prev_loc)
      {
        if constexpr (is_move)
        {
          content = next.extract();
        }
        else
        {
          next->field_inc();
          content = next.borrow();

          if (prev_loc.is_region())
            prev_loc.to_region()->stack_inc();
        }

        return ValueTransfer(prev);
      }

      // Unparent the outgoing region before trying to add the incoming
      // reference, since add_reference may drag into a region that needs
      // the cown ownership slot freed.
      Region* prev_region = nullptr;

      if (prev_loc.is_region())
      {
        prev_region = prev_loc.to_region();
        prev_region->stack_inc();
        prev_region->clear_cown_owner();
      }

      if (!add_reference<is_move>(next))
      {
        // Restore outgoing region ownership on failure.
        if (prev_region)
        {
          prev_region->set_cown_owner(this, content.get_header());
          prev_region->stack_dec();
        }

        Value::error(Error::BadStore);
      }

      return ValueTransfer(prev);
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };

  struct VbciObjectModel
  {
    using Cown = vbci::Cown;

    static verona::rt::CownSchedulerState<VbciObjectModel>&
    get_cown_scheduler_state(Cown& cown) noexcept
    {
      return cown.scheduler_state;
    }

    static void acquire(Cown& cown) noexcept
    {
      cown.inc();
    }

    static void release(Cown& cown) noexcept
    {
      cown.dec();
    }

    static uintptr_t get_cown_identity(const Cown& cown) noexcept
    {
      return reinterpret_cast<uintptr_t>(&cown);
    }
  };

  using BehaviourCore = verona::rt::boc::BehaviourCore<VbciObjectModel>;
}
