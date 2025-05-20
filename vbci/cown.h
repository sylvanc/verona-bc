#pragma once

#include "header.h"
#include "program.h"
#include "region.h"
#include "value.h"

#include <format>
#include <verona.h>

namespace vbci
{
  struct Cown : public verona::rt::VCown<Cown>
  {
  private:
    Id type_id;
    Value content;

    Cown(Id type_id) : type_id(type_id)
    {
      inc();
    }

  public:
    static Cown* create(Id type_id)
    {
      if (type::is_cown(type_id))
        throw Value(Error::BadType);

      return new Cown(type_id);
    }

    Id cown_type_id()
    {
      return type::cown(type_id);
    }

    Id content_type_id()
    {
      return type_id;
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

      if (move)
        next = std::move(v);
      else
        next = v;

      // Allow any cown to contain an error.
      if (
        !next.is_error() && !Program::get().typecheck(next.type_id(), type_id))
        next = Value(Error::BadType);

      auto prev_loc = content.location();
      auto next_loc = next.location();

      if (is_stack(next_loc))
      {
        // Can't store a stack value in a cown.
        next = Value(Error::BadStore);
      }
      else if (is_region(next_loc) && (next_loc != prev_loc))
      {
        // If the new value is in a different region, we need to check that the
        // region has no parent.
        auto r = to_region(next_loc);

        // It doesn't matter what the stack RC is, because all stack RC will be
        // gone by the time this cown is available to any other behavior.
        if (r->has_parent())
          next = Value(Error::BadStore);
        else
          r->set_parent();
      }

      if (next.is_error())
        LOG(Debug) << next.to_string();

      auto prev = std::move(content);
      content = std::move(next);

      // Clear prev region parent if it's different from next.
      if (is_region(prev_loc) && (prev_loc != next_loc))
        to_region(prev_loc)->clear_parent();

      return prev;
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };
}
