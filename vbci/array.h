#pragma once

#include "header.h"
#include "program.h"
#include "writebarrier.h"

#include <cstring>
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
    ValueTransfer exchange(size_t idx, Reg<is_move> v)
    {
      if (!Program::get().subtype(v->type_id(), content_type_id()))
        Value::error(Error::BadType);

      void* addr = reinterpret_cast<uint8_t*>(this + 1) + (stride * idx);
      return writebarrier::exchange<is_move>(
        location(), addr, value_type, std::forward<Reg<is_move>>(v));
    }

    bool is_primitive() const
    {
      return value_type != ValueType::Object &&
        value_type != ValueType::Array && value_type != ValueType::Invalid &&
        value_type != ValueType::Cown;
    }

    void bulk_copy(size_t dst_off, Array* src, size_t src_off, size_t len)
    {
      if (len == 0)
        return;

      assert(dst_off + len <= size);
      assert(src_off + len <= src->size);
      assert(value_type == src->value_type);
      assert(stride == src->stride);

      auto* dst_data =
        reinterpret_cast<uint8_t*>(this + 1) + (stride * dst_off);
      auto* src_data = reinterpret_cast<uint8_t*>(src + 1) + (stride * src_off);

      if (is_primitive())
      {
        std::memmove(dst_data, src_data, len * stride);
        return;
      }

      // Complex elements: per-element with write barrier.
      // The main win is already captured by the primitive memmove path above.
      // For complex types, we still avoid Verona-level loop overhead
      // (bytecode dispatch, ArrayRef, Load/Store per iteration).
      // Iterate backwards when copying forward within the same array
      // to avoid overwriting source elements.
      if (this == src && dst_off > src_off)
      {
        for (size_t i = len; i > 0; i--)
        {
          auto in_val = src->load(src_off + i - 1);
          exchange<false>(dst_off + i - 1, in_val);
        }
      }
      else
      {
        for (size_t i = 0; i < len; i++)
        {
          auto in_val = src->load(src_off + i);
          exchange<false>(dst_off + i, in_val);
        }
      }
    }

    void bulk_fill(size_t off, size_t len, const Value& fill_val)
    {
      if (len == 0)
        return;

      assert(off + len <= size);

      if (is_primitive())
      {
        auto* dst_data = reinterpret_cast<uint8_t*>(this + 1) + (stride * off);

        if (stride == 1)
        {
          uint8_t byte_val = 0;
          fill_val.to_addr(value_type, &byte_val);
          std::memset(dst_data, byte_val, len);
        }
        else
        {
          for (size_t i = 0; i < len; i++)
          {
            void* addr = dst_data + (stride * i);
            fill_val.to_addr(value_type, addr);
          }
        }
        return;
      }

      // Complex elements: per-element exchange.
      for (size_t i = 0; i < len; i++)
      {
        // Make a copy of fill_val for each exchange.
        ValueBorrow v(fill_val);
        exchange<false>(off + i, v);
      }
    }

    int bulk_compare(size_t a_off, Array* other, size_t b_off, size_t len) const
    {
      if (len == 0)
        return 0;

      assert(a_off + len <= size);
      assert(b_off + len <= other->size);
      assert(value_type == other->value_type);
      assert(stride == other->stride);

      if (!is_primitive())
        Value::error(Error::BadType);

      auto* a_data =
        reinterpret_cast<const uint8_t*>(this + 1) + (stride * a_off);
      auto* b_data =
        reinterpret_cast<const uint8_t*>(other + 1) + (stride * b_off);

      return std::memcmp(a_data, b_data, len * stride);
    }

    void trace_fn(auto&& fn)
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

            if (v.is_header())
              fn(v.get_header());
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
            writebarrier::drop(location(), prev);
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
