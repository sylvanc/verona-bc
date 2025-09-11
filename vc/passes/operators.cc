#include "../lang.h"

namespace vc
{
  PassDef operators()
  {
    PassDef p{
      "operators",
      wfPassOperators,
      dir::topdown,
      {
        // Try.
        In(Expr) * (T(Try) << End) * ExprPat[Expr] >>
          [](Match& _) { return Try << (Expr << _(Expr)); },

        // Prefix operator.
        In(Expr) * T(Op)[Op] * ExprPat[Expr] >>
          [](Match& _) {
            return CallDyn << (Method << (Expr << _(Expr)) << (_(Op) / Ident)
                                      << (_(Op) / TypeArgs))
                           << Args;
          },

        // Infix operator.
        In(Expr) * ExprPat[Expr] * T(Op)[Op] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << (Method << (Expr << _(Expr)) << (_(Op) / Ident)
                                      << (_(Op) / TypeArgs))
                           << seq_to_args(_(ExprSeq));
          },

        In(Expr) * ExprPat[Lhs] * T(Op)[Op] * ExprPat[Rhs] >>
          [](Match& _) {
            return CallDyn << (Method << (Expr << _(Lhs)) << (_(Op) / Ident)
                                      << (_(Op) / TypeArgs))
                           << (Args << (Expr << _(Rhs)));
          },
      }};

    p.post([](auto top) {
      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
        }
        else if (node->get_contains_error())
        {
          // Do nothing.
        }
        else if (node == Expr)
        {
          if (node->size() != 1)
          {
            node->parent()->replace(node, err(node, "Expected an expression"));
            ok = false;
          }
        }
        else if (node->in({Ref, Try}))
        {
          if (node->empty())
          {
            node->parent()->replace(node, err(node, "Expected an expression"));
            ok = false;
          }
        }

        return ok;
      });

      return 0;
    });

    return p;
  }
}
