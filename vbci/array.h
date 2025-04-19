#pragma once

#include "header.h"

namespace vbci
{
  struct Array : public Header
  {
  private:
    // TODO: array content type
    size_t size;
    Value data[0];

    Array(Location loc, size_t size) : Header(loc), size(size)
    {
      for (size_t i = 0; i < size; i++)
        data[i] = Value();
    }

  public:
    static Array* create(void* mem, Location loc, size_t size)
    {
      return new (mem) Array(loc, size);
    }

    static size_t size_of(size_t size)
    {
      return sizeof(Array) + (size * sizeof(Value));
    }

    Value load(size_t idx)
    {
      return data[idx];
    }

    Value store(ArgType arg_type, size_t idx, Value& v)
    {
      if (idx >= size)
        throw Value(Error::BadStore);

      return base_store(arg_type, data[idx], v);
    }
  };
}
