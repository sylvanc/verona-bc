#pragma once

#include "ident.h"
#include "value.h"

namespace vbci
{
  struct Cown
  {
    // TODO: more complex type?
    TypeId type_id;
    ARC arc;
    Value content;

    void inc()
    {
      arc++;
    }

    void dec()
    {
      // TODO: free at zero
      arc--;
    }

    Value store(Value& v)
    {
      // TODO: type_check, safe_store
      auto prev = std::move(content);
      content = std::move(v);
      return prev;
    }
  };
}
