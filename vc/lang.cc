#include "lang.h"

namespace vc
{
  Node seq_to_args(Node seq)
  {
    assert(seq == ExprSeq);

    if (seq->size() == 1)
    {
      auto expr = seq->front();

      if (expr == List)
        return Args << *expr;

      if (expr->size() == 1)
      {
        auto e = expr->front();

        if (e == Tuple)
          return Args << *e;
      }
    }

    return Args << (Expr << seq);
  }
}
