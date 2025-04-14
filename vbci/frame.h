#pragma once

#include "types.h"
#include "value.h"

#include <vector>

namespace vbci
{
  struct Frame
  {
    Function* func;
    Location frame_id;
    std::vector<Value>& locals;
    size_t base;
    PC pc;
    Local dst;
    Condition condition;

    Value& local(Local idx)
    {
      idx += base;

      if ((idx < base) || (idx >= locals.size()))
        throw Value(Error::StackOutOfBounds);

      return locals.at(idx);
    }

    void drop(Local arg_base = 0)
    {
      for (size_t i = arg_base; i < registers; i++)
        locals[base + i].drop();
    }
  };

  using Stack = std::vector<Frame>;
}
