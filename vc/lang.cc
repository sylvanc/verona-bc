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

  Nodes scope_path(Node node)
  {
    Nodes path;
    auto s = node;

    while (s && (s != Top))
    {
      path.push_back(s);
      s = s->parent({Top, ClassDef, TypeAlias, Function});
    }

    std::reverse(path.begin(), path.end());
    return path;
  }

  Node find_def(Node top, const Node& name)
  {
    assert(name->in({FuncName, TypeName}));
    Node def = top;

    for (auto& elem : *name)
    {
      assert(elem == NameElement);
      auto defs = def->look((elem / Ident)->location());

      if (defs.empty())
        return {};

      def = defs.front();
    }

    return def;
  }

  Node fq_typeparam(const Nodes& path, Node tp)
  {
    Node tn = TypeName;

    for (auto& s : path)
      tn << (NameElement << clone(s / Ident) << TypeArgs);

    tn << (NameElement << clone(tp / Ident) << TypeArgs);
    return tn;
  }

  Node fq_typeargs(const Nodes& path, Node tps)
  {
    Node ta = TypeArgs;

    for (auto& tp : *tps)
      ta << (Type << fq_typeparam(path, tp));

    return ta;
  }

  Node make_selftype(Node node, bool fq)
  {
    auto cls = node->parent(ClassDef);
    auto tps = cls / TypeParams;
    auto path = scope_path(cls);
    auto ta = fq ? fq_typeargs(path, tps) : make_typeargs(tps);

    Node tn = TypeName;

    for (auto& s : path)
    {
      if (s == cls)
        tn << (NameElement << clone(cls / Ident) << ta);
      else
        tn << (NameElement << clone(s / Ident) << TypeArgs);
    }

    return Type << tn;
  }
}
