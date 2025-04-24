#pragma once

#include "ident.h"
#include "value.h"

#include <format>

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
      return content.swap(arg_type, false, v);
    }

    std::string to_string()
    {
      return std::format("cown: {}", static_cast<void*>(this));
    }
  };
}
