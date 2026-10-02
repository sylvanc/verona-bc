#include "diagnostics.h"

namespace virc
{
  using namespace trieste;

  Node err(const std::string& msg)
  {
    return Error << errmsg(msg);
  }

  Node err(Node node, const std::string& msg)
  {
    return Error << errmsg(msg) << errloc(node);
  }

  Node errmsg(const std::string& msg)
  {
    return ErrorMsg ^ msg;
  }

  Node errloc(Node node)
  {
    auto loc = node;

    while (
      loc &&
      (!loc->location().source || loc->location().source->origin().empty()))
    {
      loc = loc->parent();
    }

    if (loc)
      loc = loc->type() ^ loc;

    return loc;
  }
}