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
    TypeId type_id;
    Value content;

    Cown(TypeId type_id) : type_id(type_id.cown())
    {
      inc();
    }

  public:
    static Cown* create(TypeId type_id)
    {
      return new Cown(type_id);
    }

    TypeId cown_type_id()
    {
      return type_id;
    }

    TypeId content_type_id()
    {
      return type_id.uncown();
    }

    void inc()
    {
      acquire(this);
    }

    void dec()
    {
      release(this);
    }

    Value load()
    {
      return content;
    }

    Value store(bool move, Value& v)
    {
      Value next;
      bool unparent_prev = true;
      Region* nr;

      if (move)
        next = std::move(v);
      else
        next = v;

      // Allow any cown to contain an error.
      if (!next.is_error() && !(next.type_id() < content_type_id()))
        next = Value(Error::BadType);

      auto prev_loc = content.location();
      auto next_loc = next.location();

      // Can't store a stack value in a cown.
      if (loc::is_stack(next_loc))
      {
        next = Value(Error::BadStore);
      }
      else if (loc::is_region(next_loc) && (next_loc != prev_loc))
      {
        // It doesn't matter what the stack RC is, because all stack RC will be
        // gone by the time this cown is available to any other behavior.
        auto r = loc::to_region(next_loc);

        if (r->is_frame_local())
        {
          // Drag a frame-local allocation to a fresh region.
          nr = Region::create(RegionType::RegionRC);
          nr->set_parent();
          auto drag_result = drag_allocation(nr, next.get_header(), prev_loc);
          if (!drag_result.first)
          {
            next = Value(Error::BadStore);
            nr->free_region();
          }
          else
          {
            unparent_prev = drag_result.second;
            next_loc = next.location();
          }
        }
        else if (r->has_parent())
        {
          // If the region has a parent, it can't be stored.
          next = Value(Error::BadStore);
        }
        else
        {
          // Set the region parent to this cown.
          r->set_parent();
        }
      }

      if (next.is_error())
        LOG(Debug) << next.to_string();

      auto prev = std::move(content);
      content = std::move(next);
      // Clear prev region parent if it's different from next, and assign a new
      // parent if drag occured and there was a reference from frame local to
      // previous so now the newly created region is the parent of the prev
      // region
      if (loc::is_region(prev_loc) && (prev_loc != next_loc))
      {
        loc::to_region(prev_loc)->clear_parent();
        if (!unparent_prev)
          loc::to_region(prev_loc)->set_parent(nr);
      }

      return prev;
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };
}
