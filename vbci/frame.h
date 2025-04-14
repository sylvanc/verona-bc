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

    Value& arg(Local idx)
    {
      idx += (base + registers);

      if ((idx < base) || (idx >= locals.size()))
        throw Value(Error::StackOutOfBounds);

      return locals.at(idx);
    }

    void drop()
    {
      // TODO: this would be more efficient if we knew how many locals to drop.
      // It would also let us have smaller value stacks.
      for (size_t i = 0; i < registers; i++)
        locals[base + i].drop();
    }
  };

  using Stack = std::vector<Frame>;
}
