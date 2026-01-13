#include "frame.h"

#include "function.h"
#include "object.h"

namespace vbci
{
  Frame::Frame(
    Function* func,
    Location frame_id,
    Stack::Idx save,
    std::vector<Register>& locals,
    size_t base,
    std::vector<Object*>& finalize,
    size_t finalize_base,
    PC pc,
    size_t dst,
    CallType calltype)
  : func(func),
    frame_id(frame_id),
    save(save),
    locals(locals),
    base(base),
    finalize(finalize),
    finalize_base(finalize_base),
    finalize_top(finalize_base),
    pc(pc),
    dst(dst),
    calltype(calltype)
  {
    region.set_frame_local_owner();
  }

  Register& Frame::local(size_t idx)
  {
    assert(idx < func->registers && "Local index out of bounds");

    auto i = base + idx;

    while (i >= locals.size())
      locals.resize(locals.size() * 2);

    return locals.at(i);
  }

  Register& Frame::arg(size_t idx)
  {
    auto i = base + func->registers + idx;

    while (i >= locals.size())
      locals.resize(locals.size() * 2);

    return locals.at(i);
  }

  std::span<Register> Frame::args(size_t args)
  {
    return std::span<Register>{
      locals.data() + base + func->registers,
      locals.data() + base + func->registers + args};
  }

  void Frame::push_finalizer(Object* obj)
  {
    auto finalizer = obj->finalizer();

    if (finalizer == nullptr)
      return;

    if (finalize_base == finalize_top)
      finalize.emplace_back(obj);
    else
      finalize.at(finalize_top) = obj;

    finalize_top++;
  }

  void Frame::drop()
  {
    for (size_t i = 0; i < func->registers; i++)
    {
      if (locals[base + i]->is_invalid())
        continue;
      LOG(Trace) << "Dropping frame local " << i << "  contents ("
                 << locals[base + i] << ")";
      locals[base + i].clear();
      assert(locals[base + i]->is_invalid());
    }
  }

  void Frame::drop_args(size_t& args)
  {
    for (size_t i = 0; i < args; i++)
      locals[base + func->registers + i].clear();

    args = 0;
  }

  RegionRC& Frame::get_frame_local_region()
  {
    return region;
  }
}