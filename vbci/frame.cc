#include "frame.h"

#include "function.h"
#include "object.h"

namespace vbci
{
  Frame::Frame(
    Function* func,
    Location frame_id,
    Stack::Idx save,
    std::vector<Value>& locals,
    size_t base,
    std::vector<Object*>& finalize,
    size_t finalize_base,
    PC pc,
    Local dst,
    Condition condition)
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
    condition(condition)
  {}

  Value& Frame::local(Local idx)
  {
    auto i = base + idx;

    while (i >= locals.size())
      locals.resize(locals.size() * 2);

    return locals.at(i);
  }

  Value& Frame::arg(Local idx)
  {
    auto i = base + func->registers + idx;

    while (i >= locals.size())
      locals.resize(locals.size() * 2);

    return locals.at(i);
  }

  std::span<Value> Frame::args(size_t args)
  {
    return std::span<Value>{
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
      locals[base + i].drop();
  }

  void Frame::drop_args(size_t& args)
  {
    for (size_t i = 0; i < args; i++)
      locals[base + func->registers + i].drop();

    args = 0;
  }
}
