#pragma once

#include "ident.h"

namespace vbci
{

  struct Location
  {
    static constexpr auto None = uintptr_t(0x0);
    static constexpr auto Stack = uintptr_t(0x1);
    static constexpr auto Immutable = uintptr_t(0x2);
    static constexpr auto Pending = uintptr_t(0x3);
    static constexpr auto Mask = uintptr_t(0x3);
    static constexpr auto Immortal = uintptr_t(-1) & ~Stack;
    static constexpr auto FrameInc = uintptr_t(0x4);

  private:
    uintptr_t value;

    constexpr Location() : value(None) {}
    constexpr Location(uintptr_t v) : value(v) {}

  public:
    static constexpr Location from_raw(uintptr_t raw)
    {
      return Location(raw);
    }

    static constexpr Location none()
    {
      return Location(None);
    };

    static constexpr Location stack()
    {
      return Location(Stack);
    };

    static constexpr Location immutable()
    {
      return Location(Immutable);
    };

    static constexpr Location immortal()
    {
      return Location(Immortal);
    };

    explicit Location(Region* r) : value(reinterpret_cast<uintptr_t>(r)) {}

    constexpr uintptr_t raw() const
    {
      return value;
    }

    constexpr bool is_none() const
    {
      return value == None;
    }

    bool operator==(const Location& other) const
    {
      return value == other.value;
    }

    bool operator!=(const Location& other) const
    {
      return value != other.value;
    }

    bool operator<(const Location& other) const
    {
      return value < other.value;
    }

    bool operator<=(const Location& other) const
    {
      return value <= other.value;
    }

    bool operator>(const Location& other) const
    {
      return value > other.value;
    }

    bool operator>=(const Location& other) const
    {
      return value >= other.value;
    }

    bool no_rc() const
    {
      return ((value & Stack) != 0) || (value == Immortal);
    }

    bool is_region() const
    {
      return (value != None) && ((value & Mask) == 0);
    }

    bool is_stack() const
    {
      return (value & Mask) == Stack;
    }

    bool is_immutable() const
    {
      return (value & Mask) == Immutable;
    }

    bool is_pending() const
    {
      return (value & Mask) == Pending;
    }

    Region* to_region() const
    {
      assert(is_region());
      return reinterpret_cast<Region*>(value);
    }

    Header* to_scc() const
    {
      assert(is_immutable());
      return reinterpret_cast<Header*>(value & ~Immutable);
    }

    Location pending() const
    {
      assert(is_region());
      return Location(value | Pending);
    }

    Location unpending() const
    {
      assert(is_pending());
      return Location(value & ~Pending);
    }

    Location next_stack_level() const
    {
      assert(is_stack());
      return Location(value + FrameInc);
    }
  };

  bool drag_allocation(Region* r, Header* h);
}
