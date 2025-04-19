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
    std::vector<std::pair<Object*, Function*>>& finalize;
    size_t finalize_base;
    size_t finalize_top;
    PC pc;
    Local dst;
    Condition condition;

    Frame(
      Function* func,
      Location frame_id,
      Stack::Idx save,
      std::vector<Value>& locals,
      size_t base,
      std::vector<std::pair<Object*, Function*>>& finalize,
      size_t finalize_base,
      PC pc,
      Local dst,
      Condition condition);

    Value& local(Local idx);
    Value& arg(Local idx);
    void push_finalizer(Object* obj, Function* finalizer);
    void drop();
  };
}
