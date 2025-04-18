#include "frame.h"

#include "function.h"

namespace vbci
{
  Frame::Frame(
    Function* func,
    Location frame_id,
    Stack::Idx save,
    std::vector<Value>& locals,
    size_t base,
    std::vector<std::pair<Object*, Function*>>& finalize,
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

    if ((i < base) || (i >= locals.size()))
      throw Value(Error::StackOutOfBounds);

    return locals.at(i);
  }

  Value& Frame::arg(Local idx)
  {
    auto i = base + func->registers + idx;

    if ((i < base) || (i >= locals.size()))
      throw Value(Error::StackOutOfBounds);

    return locals.at(i);
  }

  void Frame::push_finalizer(Object* obj, Function* finalizer)
  {
    if (finalizer == nullptr)
      return;

    if (finalize_base == finalize_top)
      finalize.emplace_back(obj, finalizer);
    else
      finalize.at(finalize_top) = {obj, finalizer};

    finalize_top++;
  }

  void Frame::drop()
  {
    for (size_t i = 0; i < func->registers; i++)
      locals[base + i].drop();
  }
}
