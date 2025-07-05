#pragma once

#include "stack.h"
#include "types.h"
#include "value.h"
#include "region_rc.h"

#include <span>
#include <vector>

namespace vbci
{
  struct Frame
  {
    RegionRC region;
    Function* func;
    Location frame_id;
    Stack::Idx save;
    std::vector<Value>& locals;
    size_t base;
    std::vector<Object*>& finalize;
    size_t finalize_base;
    size_t finalize_top;
    PC pc;
    size_t dst;
    CallType calltype;

    Frame(
      Function* func,
      Location frame_id,
      Stack::Idx save,
      std::vector<Value>& locals,
      size_t base,
      std::vector<Object*>& finalize,
      size_t finalize_base,
      PC pc,
      size_t dst,
      CallType calltype);

    Value& local(size_t idx);
    Value& arg(size_t idx);
    std::span<Value> args(size_t args);
    void push_finalizer(Object* obj);
    void drop();
    void drop_args(size_t& args);
  };
}
