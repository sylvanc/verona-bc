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
      Id type_id,
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
      Id type_id,
      ValueType value_type,
      size_t size,
      size_t stride)
    {
      if (type::is_array(type_id))
        throw Value(Error::BadType);

      return new (mem)
        Array(loc, type::array(type_id), value_type, size, stride);
    }

    static size_t size_of(size_t size, size_t stride)
    {
      return sizeof(Array) + (size * stride);
    }

    Id content_type_id()
    {
      return type::unarray(get_type_id());
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

    Value load(size_t idx)
    {
      void* addr = reinterpret_cast<uint8_t*>(this + 1) + (stride * idx);
      return Value::from_addr(value_type, addr);
    }

    Value store(bool move, size_t idx, Value& v)
    {
      if (!Program::get().typecheck(v.type_id(), content_type_id()))
        throw Value(Error::BadType);

      if (!safe_store(v))
        throw Value(Error::BadStore);

      void* addr = reinterpret_cast<uint8_t*>(this + 1) + (stride * idx);
      auto prev = Value::from_addr(value_type, addr);
      region_store(prev, v);
      v.to_addr(move, addr);
      return prev;
    }

    void dec(bool reg)
    {
      if (base_dec(reg))
        return;

      finalize();

      if (location() == Immutable)
        delete this;
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
      if (location() == Immortal)
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
            load(i).field_drop();
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
  };
}
