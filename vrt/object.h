#pragma once

#include "../include/vrt/object.h"
#include "header.h"

#include <cstddef>

namespace vrt
{
  /** Runtime object stored immediately before its exposed payload. */
  struct Object final : Header
  {
    const Class* cls = nullptr;

  private:
    Object(std::byte* allocation, const Class* cls);

  public:
    static constexpr size_t singleton_payload_offset()
    {
      return sizeof(Object);
    }

    static constexpr size_t singleton_storage_size()
    {
      return singleton_payload_offset() + 1;
    }

    static Object* create_singleton(std::byte* storage, const Class* cls);

    void* get_payload()
    {
      return this + 1;
    }

    const void* get_payload() const
    {
      return this + 1;
    }
  };

  /** Construct one immortal object header in compiler-emitted storage. */
  void init_singleton(void* storage, const Class* cls);
}
