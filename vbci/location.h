#pragma once

#include "ident.h"

namespace vbci
{
  namespace loc
  {
    static constexpr auto None = uintptr_t(0x0);
    static constexpr auto Stack = uintptr_t(0x1);
    static constexpr auto Immutable = uintptr_t(0x2);
    static constexpr auto Pending = uintptr_t(0x3);
    static constexpr auto Mask = uintptr_t(0x3);
    static constexpr auto Immortal = uintptr_t(-1) & ~Stack;
    static constexpr auto FrameInc = uintptr_t(0x4);

    inline bool no_rc(Location loc)
    {
      return ((loc & Stack) != 0) || (loc == Immortal);
    }

    inline bool is_region(Location loc)
    {
      return (loc != None) && ((loc & Mask) == 0);
    }

    inline bool is_stack(Location loc)
    {
      return (loc & Mask) == Stack;
    }

    inline bool is_immutable(Location loc)
    {
      return (loc & Mask) == Immutable;
    }

    inline bool is_pending(Location loc)
    {
      return (loc & Mask) == Pending;
    }

    inline Region* to_region(Location loc)
    {
      assert(is_region(loc));
      return reinterpret_cast<Region*>(loc);
    }

    inline Header* to_scc(Location loc)
    {
      assert(is_immutable(loc));
      return reinterpret_cast<Header*>(loc & ~Immutable);
    }

    inline Location pending(Location loc)
    {
      assert(is_region(loc));
      return loc | Pending;
    }

    inline Location unpending(Location loc)
    {
      assert(is_pending(loc));
      return loc & ~Pending;
    }
  }

  std::pair<bool,bool> drag_allocation(Region* r, Header* h, Location pr);
}
