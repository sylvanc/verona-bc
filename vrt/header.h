#pragma once

#include "location.h"

#include <cstddef>
#include <cstdint>

namespace vrt
{
  /** State shared by every managed VRT allocation. */
  struct Header
  {
  private:
    Location loc;

  public:
    static constexpr uint64_t magic_value = 0x5652544845414445ULL;

    uint64_t magic = magic_value;
    uintptr_t type_id;
    uintptr_t reference_count = 1;
    std::byte* allocation = nullptr;
    bool finalizing = false;

    Header(Location location, uintptr_t type_id)
    : loc(location), type_id(type_id)
    {}

    Location location() const
    {
      return loc;
    }

    uintptr_t get_type_id() const
    {
      return type_id;
    }
  };
}
