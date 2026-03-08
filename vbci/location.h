#pragma once

#include "ident.h"

namespace vbci
{
  struct Region;
  struct Header;

  struct Location
  {
    static constexpr auto Stack = uintptr_t(0x1);
    static constexpr auto Immutable = uintptr_t(0x2);
    static constexpr auto Pending = uintptr_t(0x3);
    static constexpr auto Immortal = uintptr_t(0x4);
    static constexpr auto SccPtr = uintptr_t(0x5);
    static constexpr auto Mask = uintptr_t(0x7);
    static constexpr auto FrameInc = uintptr_t(0x8);

  private:
    uintptr_t value;

    // Location objects are intentionally only constructible via the public
    // static factory methods (and internal helpers). The default constructor
    // is deleted and the raw uintptr_t constructor is kept private to enforce
    // invariants on the encoded location value.
    Location() = delete;
    constexpr Location(uintptr_t v) : value(v) {}

  public:
    static constexpr Location from_raw(uintptr_t raw)
    {
      return Location(raw);
    }

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
      return is_stack() || (value == Immortal);
    }

    bool is_region() const
    {
      return (value & Mask) == 0;
    }

    bool is_stack() const
    {
      return (value & Mask) == Stack;
    }

    bool is_immortal() const
    {
      return (value & Mask) == Immortal;
    }

    bool is_immutable() const
    {
      return (value & Mask) == Immutable;
    }

    bool is_scc_ptr() const
    {
      return (value & Mask) == SccPtr;
    }

    bool is_pending() const
    {
      return (value & Mask) == Pending;
    }

    Region* to_region() const;

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

    size_t stack_index() const
    {
      assert(is_stack());
      return (value - Stack) / FrameInc;
    }

    static Location scc_ptr(Header* h)
    {
      return Location(SccPtr | reinterpret_cast<uintptr_t>(h));
    }

    Header* scc_target() const
    {
      assert(is_scc_ptr());
      return reinterpret_cast<Header*>(value & ~Mask);
    }
  };

  template <bool is_move>
  bool drag_allocation(
    Region* dest, Header* h, Region* ignore_parent = nullptr);
}
