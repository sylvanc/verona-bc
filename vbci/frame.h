#pragma once

#include "stack.h"
#include "types.h"
#include "value.h"

#include <vector>

namespace vbci
{
  struct Frame
  {
    Function* func;
    Location frame_id;
    Stack::Idx save;
    std::vector<Value>& locals;
    size_t base;
    PC pc;
    Local dst;
    Condition condition;

    Value& local(Local idx);
    Value& arg(Local idx);
    void drop();
  };
}
