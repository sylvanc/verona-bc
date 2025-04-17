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

    Value& local(Local idx);
    Value& arg(Local idx);
    void drop();
  };

  using Stack = std::vector<Frame>;
}
