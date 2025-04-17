#include "frame.h"

#include "function.h"

namespace vbci
{
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

  void Frame::drop()
  {
    for (size_t i = 0; i < func->registers; i++)
      locals[base + i].drop();
  }
}
