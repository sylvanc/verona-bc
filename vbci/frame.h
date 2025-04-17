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
      auto i = base + idx;

      if ((i < base) || (i >= locals.size()))
        throw Value(Error::StackOutOfBounds);

      return locals.at(i);
    }

    Value& arg(Local idx)
    {
      auto i = base + MaxRegisters + idx;

      if ((i < base) || (i >= locals.size()))
        throw Value(Error::StackOutOfBounds);

      return locals.at(i);
    }

    void drop()
    {
      // TODO: this would be more efficient if we knew how many locals to drop.
      // It would also let us have smaller value stacks.
      for (size_t i = 0; i < MaxRegisters; i++)
        locals[base + i].drop();
    }
  };

  using Stack = std::vector<Frame>;
}
