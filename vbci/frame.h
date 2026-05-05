#pragma once

#include "region.h"
#include "register.h"
#include "stack.h"
#include "value.h"

#include <span>
#include <vector>

namespace vbci
{
  struct Frame
  {
    Region* region;
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
    Location raise_target;

    Frame(
      Function* func,
      Location frame_id,
      Stack::Idx save,
      std::vector<Register>& locals,
      size_t base,
      std::vector<Object*>& finalize,
      size_t finalize_base,
      PC pc,
      size_t dst);

    Register& local(size_t idx);
    Register& arg(size_t idx);
    std::span<Register> args(size_t args);
    void push_finalizer(Object* obj);
    void drop();
    void drop_args(size_t& args);
    Region& get_frame_local_region() const;
  };
}
