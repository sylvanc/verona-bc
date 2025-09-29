#include "lang.h"

#include <vbcc/from_chars.h>

namespace vc
{
  size_t parse_int(Node node)
  {
    auto view = node->location().view();
    auto first = view.data();
    auto last = first + view.size();

    size_t i;
    std::from_chars(first, last, i, 10);
    return i;
  }

  Node seq_to_args(Node seq)
  {
    assert(seq == ExprSeq);

    // Empty parentheses.
    if (seq->empty())
      return Args;

    if (seq->size() == 1)
    {
      auto expr = seq->front();

      // Turn a comma-delimited list into separate Args.
      if (expr == List)
        return Args << *expr;

      if (expr->size() == 1)
      {
        auto e = expr->front();

        // Turn a tuple into separate Args.
        if (e == Tuple)
          return Args << *e;
      }
    }

    // Treat this as a single argument.
    return Args << (Expr << seq);
  }

  Node make_typeargs(Node typeparams)
  {
    Node ta = TypeArgs;

    for (auto& tp : *typeparams)
    {
      ta
        << (Type
            << (TypeName << (TypeElement << clone(tp / Ident) << TypeArgs)));
    }

    return ta;
  }

  Node make_selftype(Node node)
  {
    auto cls = node->parent(ClassDef);
    auto tps = cls / TypeParams;
    return Type
      << (TypeName
          << (TypeElement << clone(cls / Ident) << make_typeargs(tps)));
  }
}
