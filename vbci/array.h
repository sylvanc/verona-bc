#pragma once

#include "header.h"

namespace vbci
{
  struct Array : public Header
  {
    // TODO: array content type
    size_t size;
    Value data[0];

    static Array* create(Location loc, size_t size)
    {
      auto arr =
        static_cast<Array*>(malloc(sizeof(Array) + (size * sizeof(Value))));
      arr->loc = loc;
      arr->rc = 1;
      arr->size = size;

      for (size_t i = 0; i < size; i++)
        arr->data[i] = Value();

      return arr;
    }

    Value store(ArgType arg_type, size_t idx, Value& v)
    {
      if (idx >= size)
        throw Value(Error::BadStore);

      return base_store(arg_type, data[idx], v);
    }
  };
}
