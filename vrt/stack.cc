#include "stack.h"

#include "array.h"
#include "failure.h"
#include "header.h"
#include "object.h"
#include "program.h"

#include <algorithm>
#include <limits>
#include <new>
#include <optional>

namespace vrt
{
  namespace
  {
    std::optional<size_t> allocation_offset(
      size_t used, size_t capacity, size_t size, size_t alignment)
    {
      const auto remainder = used % alignment;
      const auto padding = remainder == 0 ? 0 : alignment - remainder;
      if ((padding > (capacity - used)) || (size > (capacity - used - padding)))
        return {};

      return used + padding;
    }
  }

  std::byte* Stack::allocate(size_t size, size_t alignment)
  {
    internal_check(
      (size != 0) && (alignment != 0) && ((alignment & (alignment - 1)) == 0) &&
        (alignment <= alignof(std::max_align_t)),
      Failure::invalid_frame_state);

    size_t chunk_index = active_chunk;
    std::optional<size_t> offset;
    if (!chunks.empty())
    {
      offset = allocation_offset(
        chunks[chunk_index].used,
        chunks[chunk_index].capacity,
        size,
        alignment);
    }

    if (!offset)
    {
      for (chunk_index = active_chunk + 1; chunk_index < chunks.size();
           chunk_index++)
      {
        auto& chunk = chunks[chunk_index];
        internal_check(chunk.used == 0, Failure::invalid_frame_state);
        offset = allocation_offset(chunk.used, chunk.capacity, size, alignment);
        if (offset)
          break;
      }
    }

    if (!offset)
    {
      internal_check(
        size <= (std::numeric_limits<size_t>::max() - (alignment - 1)),
        Failure::invalid_frame_state);
      const auto capacity =
        std::max(default_chunk_size, size + (alignment - 1));
      auto data =
        std::unique_ptr<std::byte[]>{new (std::nothrow) std::byte[capacity]};
      if (data == nullptr)
        fail(Failure::out_of_memory);

      try
      {
        chunks.push_back(Chunk{std::move(data), capacity});
      }
      catch (const std::bad_alloc&)
      {
        fail(Failure::out_of_memory);
      }

      chunk_index = chunks.size() - 1;
      offset = allocation_offset(0, capacity, size, alignment);
      internal_check(offset.has_value(), Failure::invalid_frame_state);
    }

    auto& chunk = chunks[chunk_index];
    try
    {
      allocations.push_back(Allocation{chunk_index, chunk.used, active_chunk});
    }
    catch (const std::bad_alloc&)
    {
      fail(Failure::out_of_memory);
    }

    active_chunk = chunk_index;
    chunk.used = *offset + size;
    return chunk.data.get() + *offset;
  }

  size_t Stack::mark() const
  {
    return allocations.size();
  }

  size_t Stack::finalizer_mark() const
  {
    return finalizers.size();
  }

  Object* Stack::object(Location location, const Class* cls)
  {
    auto* allocation =
      allocate(Object::size_of(cls), alignof(std::max_align_t));
    auto* result = Object::create(allocation, cls, location);
    try
    {
      finalizers.push_back(result);
    }
    catch (const std::bad_alloc&)
    {
      fail(Failure::out_of_memory);
    }

    return result;
  }

  Array* Stack::array(Location location, uintptr_t type_id, uintptr_t size)
  {
    const auto content_type_id = unarray(type_id);
    const auto layout = layout_type_id(content_type_id);
    auto* allocation = allocate(
      Array::size_of(size, layout.storage_size), alignof(std::max_align_t));
    auto* result = Array::create(
      allocation,
      location,
      type_id,
      layout.value_type,
      size,
      layout.storage_size);
    try
    {
      finalizers.push_back(result);
    }
    catch (const std::bad_alloc&)
    {
      fail(Failure::out_of_memory);
    }

    return result;
  }

  void Stack::unwind(size_t stack_mark, size_t finalizer_mark)
  {
    internal_check(
      (stack_mark <= allocations.size()) &&
        (finalizer_mark <= finalizers.size()),
      Failure::invalid_frame_state);

    for (size_t index = finalizer_mark; index < finalizers.size(); index++)
      finalizers[index]->finalize();

    for (size_t index = finalizer_mark; index < finalizers.size(); index++)
      finalizers[index]->destroy_storage();

    finalizers.resize(finalizer_mark);

    while (allocations.size() > stack_mark)
    {
      const auto allocation = allocations.back();
      chunks[allocation.chunk].used = allocation.previous_used;
      active_chunk = allocation.previous_active_chunk;
      allocations.pop_back();
    }

    if (allocations.empty())
      active_chunk = 0;
  }
}