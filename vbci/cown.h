#pragma once

#include "ident.h"
#include "value.h"

namespace vbci
{
  struct Cown
  {
    // TODO: cown content type
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

    Value store(ArgType arg_type, Value& v)
    {
      // TODO: safe_store
      auto prev = std::move(content);

      if (arg_type == ArgType::Move)
        content = std::move(v);
      else
        content = v;

      return prev;
    }
  };
}
