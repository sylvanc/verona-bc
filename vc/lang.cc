#include "lang.h"

namespace vc
{
  Node make_type(Match& _, NodeRange r)
  {
    return Type << (r || (TypeVar ^ _.fresh(l_typevar)));
  }

  Node make_typeargs(Node typeparams)
  {
    Node ta = TypeArgs;

    for (auto& tp : *typeparams)
    {
      ta
        << (Type
            << (TypeName << (NameElement << clone(tp / Ident) << TypeArgs)));
    }

    return ta;
  }

  Node make_selftype(Node node)
  {
    auto cls = node->parent(ClassDef);
    auto tps = cls / TypeParams;
    return Type
      << (TypeName
          << (NameElement << clone(cls / Ident) << make_typeargs(tps)));
  }
}
