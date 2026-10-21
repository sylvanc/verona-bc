#pragma once

#include "location.h"
#include "rc.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vrt/value.h>

namespace vrt
{
  struct Region;

  /** State shared by every region-managed VRT allocation. */
  struct Header
  {
  private:
    Location loc;

  public:
    static constexpr uint64_t magic_value = 0x5652544845414445ULL;

    uint64_t magic = magic_value;
    uintptr_t type_id;
    alignas(ARC::required_alignment) RC reference_count = 1;
    std::byte* allocation = nullptr;
    bool finalizing = false;
    std::atomic<bool> collecting_scc = false;

    Header(Location location, uintptr_t type_id)
    : loc(location), type_id(type_id)
    {}

    Location location() const
    {
      return loc;
    }

    void set_location(Location location)
    {
      loc = location;
    }

    /** Return the canonical representative without mutating published state. */
    Header* representative();
    const Header* representative() const;

    /** Freeze-only union-find lookup with path compression. */
    static Header* find(Header* header);

    bool try_begin_scc_collection();
    bool is_collecting_scc() const;

    Region* region() const
    {
      if (!loc.is_region())
        return nullptr;

      return loc.to_region();
    }

    uintptr_t get_type_id() const
    {
      return type_id;
    }

    RC get_rc() const;
    void set_rc(RC value);
    RC get_arc() const;
    void set_arc(RC value);
    void inc_arc();
    bool dec_arc();

    static Header* from_data(ValueType value_type, const void* data_address);

    void* data();
    const void* data() const;

    ValueType value_type() const;

    template<typename F>
    void trace_fn(F&& function);

    void finalize();
    void destroy_storage();
    void root_ref_inc();
    void root_ref_dec();
    void field_ref_inc();
    void field_ref_dec();
  };

  static_assert(
    alignof(Header) >= ARC::required_alignment,
    "Header must align reference_count for atomic_ref");
}
