#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace vrt
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

    Location() = delete;
    constexpr Location(uintptr_t value) : value(value) {}

  public:
    static constexpr Location from_raw(uintptr_t raw)
    {
      return Location(raw);
    }

    static constexpr Location stack()
    {
      return Location(Stack);
    }

    static constexpr Location immutable()
    {
      return Location(Immutable);
    }

    static constexpr Location immortal()
    {
      return Location(Immortal);
    }

    explicit Location(Region* region)
    : value(reinterpret_cast<uintptr_t>(region))
    {}

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
      auto tag = value & Mask;
      return tag == Immutable || tag == SccPtr;
    }

    bool is_scc_ptr() const
    {
      return (value & Mask) == SccPtr;
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

    static Location scc_ptr(Header* header)
    {
      return Location(SccPtr | reinterpret_cast<uintptr_t>(header));
    }

    Header* scc_target() const
    {
      assert(is_scc_ptr());
      return reinterpret_cast<Header*>(value & ~Mask);
    }
  };
}
