#pragma once

#include "header.h"
#include "value.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vrt/object.h>

namespace vrt
{
  /** Runtime array stored immediately before its exposed elements. */
  struct alignas(std::max_align_t) Array final : Header
  {
  private:
    uintptr_t size;
    uintptr_t stride;
    ValueType element_value_type;

    Array(
      Location location,
      uintptr_t type_id,
      ValueType value_type,
      uintptr_t size,
      uintptr_t stride,
      std::byte* allocation);

    Field element() const;

  public:
    static Array* create(
      std::byte* allocation,
      Location location,
      uintptr_t type_id,
      ValueType value_type,
      uintptr_t size,
      uintptr_t stride);

    static size_t size_of(uintptr_t size, uintptr_t stride);

    uintptr_t content_type_id() const;

    uintptr_t get_size() const
    {
      return size;
    }

    uintptr_t get_stride() const
    {
      return stride;
    }

    ValueType get_value_type() const
    {
      return element_value_type;
    }

    void* get_pointer()
    {
      return this + 1;
    }

    const void* get_pointer() const
    {
      return this + 1;
    }

    void* get_payload()
    {
      return get_pointer();
    }

    const void* get_payload() const
    {
      return get_pointer();
    }

    /** Return the address containing the encoded value at index. */
    void* load(uintptr_t index);
    const void* load(uintptr_t index) const;

    template<typename F>
    void trace_fn(F&& function)
    {
      if (!is_header_type(element_value_type))
        return;

      for (uintptr_t index = 0; index < size; index++)
      {
        void* payload = nullptr;
        std::memcpy(&payload, load(index), sizeof(payload));
        if (payload != nullptr)
          function(Value{element_value_type, payload}.header());
      }
    }

    void finalize();
    void destroy_storage();

    size_t allocation_size_bytes() const
    {
      return size_of(size, stride);
    }
  };

}
