#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace vrt
{
  struct Location
  {
    static constexpr auto Stack = uintptr_t(0x1);
    static constexpr auto Immortal = uintptr_t(0x4);
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

    static constexpr Location immortal()
    {
      return Location(Immortal);
    }

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

    bool is_stack() const
    {
      return (value & Mask) == Stack;
    }

    bool is_immortal() const
    {
      return (value & Mask) == Immortal;
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

  };
}
