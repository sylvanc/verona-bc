#pragma once

#include "header.h"
#include "program.h"

#include <format>

namespace vbci
{
  struct Array : public Header
  {
  private:
    size_t size;
    uint32_t stride;
    ValueType value_type;

    Array(
      Location loc,
      uint32_t type_id,
      ValueType value_type,
      size_t size,
      size_t stride)
    : Header(loc, type_id), size(size), stride(stride), value_type(value_type)
    {
      if (value_type == ValueType::Invalid)
      {
        auto data = reinterpret_cast<Value*>(this + 1);

        for (size_t i = 0; i < size; i++)
          data[i] = Value();
      }
      else
      {
        std::memset(this + 1, 0, size * stride);
      }
    }

  public:
    static Array* create(
      void* mem,
      Location loc,
      uint32_t type_id,
      ValueType value_type,
      size_t size,
      size_t stride)
    {
      return new (mem) Array(loc, type_id, value_type, size, stride);
    }

    static size_t size_of(size_t size, size_t stride)
    {
      return sizeof(Array) + (size * stride);
    }

    uint32_t content_type_id()
    {
      return Program::get().unarray(get_type_id());
    }

    size_t get_size()
    {
      return size;
    }

    void set_size(size_t new_size)
    {
      // This can only be used to shrink the apparent size of the array.
      if (new_size < size)
        size = new_size;
    }

    void* get_pointer()
    {
      return reinterpret_cast<void*>(this + 1);
    }

    ValueBorrow load(size_t idx)
    {
      void* addr = reinterpret_cast<uint8_t*>(this + 1) + (stride * idx);
      return Value::from_addr(value_type, addr);
    }

    template<bool is_move>
    void exchange(Register& dst, size_t idx, Reg<is_move> v)
    {
      if (!Program::get().subtype(v->type_id(), content_type_id()))
        Value::error(Error::BadType);

      void* addr = reinterpret_cast<uint8_t*>(this + 1) + (stride * idx);

      Header::exchange<is_move>(&dst, addr, value_type, std::forward<Reg<is_move>>(v));
    }

    /**
     * Finalises and deallocates the array, this should not be
     * called directly due to issues with re-entrancy.
     * Instead, use collect(Array*), or dec(..).
     */
    void deallocate()
    {
      finalize();

      if (location().is_immutable())
        delete[] reinterpret_cast<uint8_t*>(this);
      else
        region()->rfree(this);
    }

    void trace(std::vector<Header*>& list)
    {
      switch (value_type)
      {
        case ValueType::Object:
        case ValueType::Array:
        case ValueType::Invalid:
        {
          for (size_t i = 0; i < size; i++)
          {
            auto v = load(i);

            if (!v.is_header())
              return;

            auto h = v.get_header();

            // Only add mutable, heap allocated objects and arrays to the list.
            if (h->region())
              list.push_back(h);
          }
          break;
        }

        default:
          break;
      }
    }

    void immortalize()
    {
      if (location() == Location::immortal())
        return;

      mark_immortal();

      switch (value_type)
      {
        case ValueType::Object:
        case ValueType::Array:
        case ValueType::Invalid:
        {
          for (size_t i = 0; i < size; i++)
            load(i).immortalize();
          break;
        }

        default:
          break;
      }
    }

    void finalize()
    {
      switch (value_type)
      {
        case ValueType::Object:
        case ValueType::Array:
        case ValueType::Invalid:
        {
          for (size_t i = 0; i < size; i++)
          {
            void* addr = reinterpret_cast<uint8_t*>(this + 1) + (stride * i);
            auto prev = Value::from_addr(value_type, addr);

            field_drop(prev);
          }
          break;
        }

        default:
          break;
      }
    }

    std::string to_string()
    {
      return std::format("array[{}]: {}", size, static_cast<void*>(this));
    }

    size_t allocation_size_bytes() const
    {
      return Array::size_of(size, stride);
    }
  };
}
