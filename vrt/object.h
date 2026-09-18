#pragma once

#include "../include/vrt/object.h"
#include "header.h"

#include <cstddef>
#include <cstdint>

namespace vrt
{
  struct Region;

  /** Runtime object stored immediately before its exposed payload. */
  struct Object final : Header
  {
    const Class* cls = nullptr;

  private:
    Object(
      Region* region, const Class* cls, std::byte* allocation, bool immortal);

  public:
    static constexpr size_t singleton_payload_offset()
    {
      return sizeof(Object);
    }

    static constexpr size_t singleton_storage_size()
    {
      return singleton_payload_offset() + 1;
    }

    static size_t size_of(const Class* cls);
    static Object* create(
      std::byte* allocation,
      const Class* cls,
      Region* region,
      bool immortal = false);

    Object& init(uintptr_t argc, const void* packed_args);

    void* get_payload()
    {
      return this + 1;
    }

    const void* get_payload() const
    {
      return this + 1;
    }
  };

  void init_singleton(void* storage, const Class* cls);
  void finalize_object(Object* object);
  void destroy_object_storage(Object* object);
}
