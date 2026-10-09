#pragma once

#include "location.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace vrt
{
  struct Array;
  struct Class;
  struct Header;
  struct Object;

  /** Stable-address bump storage reclaimed at logical-frame boundaries. */
  struct Stack
  {
  private:
    static constexpr size_t default_chunk_size = 4096;

    struct Chunk
    {
      std::unique_ptr<std::byte[]> data;
      size_t capacity;
      size_t used = 0;
    };

    struct Allocation
    {
      size_t chunk;
      size_t previous_used;
      size_t previous_active_chunk;
    };

    std::vector<Chunk> chunks;
    std::vector<Allocation> allocations;
    std::vector<Header*> finalizers;
    size_t active_chunk = 0;

    std::byte* allocate(size_t size, size_t alignment);

  public:
    size_t mark() const;
    size_t finalizer_mark() const;
    Object* object(Location location, const Class* cls);
    Array* array(Location location, uintptr_t type_id, uintptr_t size);
    void unwind(size_t stack_mark, size_t finalizer_mark);
  };
}