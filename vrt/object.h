#pragma once

#include "../include/vrt/object.h"
#include "header.h"

#include <cstddef>
#include <cstdint>

namespace vrt
{
  struct Region;

  /** Runtime object stored immediately before its exposed fields. */
  struct Object final : Header
  {
    const Class* cls = nullptr;

  private:
    Object(Location location, const Class* cls, std::byte* allocation);

  public:
    static constexpr size_t singleton_data_offset()
    {
      return sizeof(Object);
    }

    static constexpr size_t singleton_storage_size()
    {
      return singleton_data_offset() + 1;
    }

    static size_t size_of(const Class* cls);
    static Object* create(
      std::byte* allocation,
      const Class* cls,
      Region* region,
      bool immortal = false);
    static Object*
    create(std::byte* allocation, const Class* cls, Location location);

    Object& init(uintptr_t argc, const void* packed_args);
    void finalize();
    void destroy_storage();

    template<typename F>
    void trace_fn(F&& function);

    void* fields()
    {
      return this + 1;
    }

    const void* fields() const
    {
      return this + 1;
    }
  };

  void init_singleton(void* storage, const Class* cls);
}
