#pragma once

#include "header.h"
#include "program.h"

#include <format>

namespace vbci
{
  struct Array : public Header
  {
  private:
    Id type_id;
    size_t size;
    uint32_t stride;
    ValueType value_type;

    Array(
      Location loc,
      Id type_id,
      ValueType value_type,
      size_t size,
      size_t stride)
    : Header(loc),
      type_id(type_id),
      size(size),
      stride(stride),
      value_type(value_type)
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

      return new (mem) Array(loc, type_id, value_type, size, stride);
    }

    static size_t size_of(size_t size, size_t stride)
    {
      return sizeof(Array) + (size * stride);
    }

    Id array_type_id()
    {
      return type::array(type_id);
    }

    Id content_type_id()
    {
      return type_id;
    }

    size_t get_size()
    {
      return size;
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
      if (!Program::get().typecheck(v.type_id(), type_id))
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

      // TODO: this will get called for an immutable and will crash.
      region()->free(this);
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
