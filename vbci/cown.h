#pragma once

#include "ident.h"
#include "value.h"

#include <format>

namespace vbci
{
  struct Cown
  {
    ARC arc;
    Id type_id;
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

    Value store(bool move, Value& v)
    {
      // TODO: safe store?
      auto prev = std::move(content);

      if (move)
        content = std::move(v);
      else
        content = v;

      return prev;
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };
}
