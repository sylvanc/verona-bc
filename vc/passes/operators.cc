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
        // Ref.
        In(Expr) * (T(Ref) << End) * LhsPat[Expr] >>
          [](Match& _) { return Ref << (Expr << _(Expr)); },

        // Try.
        In(Expr) * (T(Try) << End) * LhsPat[Expr] >>
          [](Match& _) { return Try << (Expr << _(Expr)); },

        // Hash.
        In(Expr) * (T(Hash) << End) * LhsPat[Expr] >>
          [](Match& _) { return Hash << (Expr << _(Expr)); },

        // Prefix operator.
        In(Expr) * T(Op)[Op] * LhsPat[Expr] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Expr)) << (_(Op) / Ident)
                           << (_(Op) / TypeArgs) << Args;
          },

        // Infix operator.
        In(Expr) * LhsPat[Expr] * T(Op)[Op] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Expr)) << (_(Op) / Ident)
                           << (_(Op) / TypeArgs) << (Args << *_(ExprSeq));
          },

        In(Expr) * LhsPat[Lhs] * T(Op)[Op] * LhsPat[Rhs] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(Op) / Ident)
                           << (_(Op) / TypeArgs) << (Args << (Expr << _(Rhs)));
          },

        // Infix qname.
        In(Expr) * LhsPat[Expr] * (T(Infix) << T(QName)[QName]) *
            T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return Call << _(QName)
                        << (Args << (Expr << _(Expr)) << *_(ExprSeq));
          },

        In(Expr) * LhsPat[Lhs] * (T(Infix) << T(QName)[QName]) * LhsPat[Rhs] >>
          [](Match& _) {
            return Call << _(QName)
                        << (Args << (Expr << _(Lhs)) << (Expr << _(Rhs)));
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
