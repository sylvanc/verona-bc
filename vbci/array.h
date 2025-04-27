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
    Value data[0];

    Array(Id type_id, Location loc, size_t size)
    : Header(loc), type_id(type_id), size(size)
    {
      for (size_t i = 0; i < size; i++)
        data[i] = Value();
    }

  public:
    static Array* create(void* mem, Id type_id, Location loc, size_t size)
    {
      if (type::is_array(type_id))
        throw Value(Error::BadType);

      return new (mem) Array(type_id, loc, size);
    }

    static size_t size_of(size_t size)
    {
      return sizeof(Array) + (size * sizeof(Value));
    }

    Id array_type_id()
    {
      return type::array(type_id);
    }

    Id content_type_id()
    {
      return type_id;
    }

    Value load(size_t idx)
    {
      return data[idx];
    }

    Value store(ArgType arg_type, size_t idx, Value& v)
    {
      if (idx >= size)
        throw Value(Error::BadStore);

      if (!Program::get().typecheck(v.type_id(), type_id))
        throw Value(Error::BadType);

      return base_store(arg_type, data[idx], v);
    }

    void dec(bool reg)
    {
      if (base_dec(reg))
        return;

      finalize();

      // TODO: this will get called for an immutable and will crash.
      region()->free(this);
    }

    void finalize()
    {
      for (size_t i = 0; i < size; i++)
        data[i].field_drop();
    }

    std::string to_string()
    {
      return std::format("array[{}]: {}", size, static_cast<void*>(this));
    }
  };
}
