#pragma once

#include "ident.h"
#include "region.h"
#include "value.h"

namespace vbci
{
  static constexpr auto Immortal = uintptr_t(-1);
  static constexpr auto Immutable = uintptr_t(-2);
  static constexpr auto StackAlloc = uintptr_t(0x1);

  // An object header is 16 bytes on a 64 bit system, 12 bytes on a 32 bit
  // system.
  struct Object
  {
    union
    {
      RC rc;
      ARC arc;
    };

    TypeId type_id;
    Location loc;
    Value fields[0];

    static Object* create(TypeId type_id, Location loc, size_t fields);

    bool stack_alloc()
    {
      return (loc & StackAlloc) != 0;
    }

    Region* region()
    {
      assert((loc & StackAlloc) == 0);
      assert(loc != Immutable);
      return reinterpret_cast<Region*>(loc);
    }

    void inc()
    {
      if (loc == Immutable)
      {
        arc++;
      }
      else if (stack_alloc())
      {
        // Do nothing. This will cover Immortal as well.

        // TODO: no RC for stack alloc?
        // if so, need an actual bump allocator
        // segmented stack, to avoid over-allocation
      }
      else
      {
        // RC inc comes from `load` and `dup`. As such, it's always paired with
        // a stack RC increment for the containing region.
        region()->stack_inc();

        if (region()->enable_rc())
          rc++;
      }
    }

    void dec()
    {
      if (loc == Immutable)
      {
        // TODO: free at zero
        arc--;
      }
      else if (stack_alloc())
      {
        // Do nothing. This will cover Immortal as well.
      }
      else
      {
        // RC dec comes from `drop`. As such, it's always paired with a stack RC
        // decrement for the containing region.

        // TODO: what if the region is freed here?
        region()->stack_dec();

        if (region()->enable_rc())
        {
          // TODO: free at zero
          rc--;
        }
      }
    }

    Value store(FieldIdx idx, Value& v)
    {
      // TODO: type_check, safe_store
      auto& field = fields[idx];
      auto prev = std::move(field);
      field = std::move(v);
      return prev;
    }
  };
}
