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
        // New.
        In(Expr) * (T(New) << End) * ExprPat[Expr] >>
          [](Match& _) { return New << (Expr << _(Expr)); },

        // Ref.
        In(Expr) * (T(Ref) << End) * ExprPat[Expr] >>
          [](Match& _) { return Ref << (Expr << _(Expr)); },

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

        // Unpack apply: static call.
        T(Apply) << ((T(Expr) << T(QName)[QName]) * T(Expr)++[Expr]) >>
          [](Match& _) { return Call << _(QName) << (Args << _[Expr]); },

        // Unpack apply: dynamic call.
        T(Apply) << ((T(Expr) << T(Method)[Method]) * T(Expr)++[Expr]) >>
          [](Match& _) { return CallDyn << _(Method) << (Args << _[Expr]); },

        // Unpack apply: insert call to `apply`.
        T(Apply) << (T(Expr)[Lhs] * T(Expr)++[Expr]) >>
          [](Match& _) {
            return CallDyn << (Method << _(Lhs) << (Ident ^ "apply")
                                      << TypeArgs)
                           << (Args << _[Expr]);
          },
      }};

    p.post([](auto top) {
      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
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
