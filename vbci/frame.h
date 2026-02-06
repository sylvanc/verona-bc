#pragma once

#include "stack.h"
#include "value.h"
#include "region_rc.h"
#include "register.h"

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
    std::vector<Register>& locals;
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
      std::vector<Register>& locals,
      size_t base,
      std::vector<Object*>& finalize,
      size_t finalize_base,
      PC pc,
      size_t dst,
      CallType calltype);

    Register& local(size_t idx);
    Register& arg(size_t idx);
    std::span<Register> args(size_t args);
    void push_finalizer(Object* obj);
    void drop();
    void drop_args(size_t& args);
    RegionRC& get_frame_local_region();
  };
}
