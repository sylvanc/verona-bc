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
        In(Expr) * (T(Ref) << End) * ExprDef[Expr] >>
          [](Match& _) { return Ref << _(Expr); },

        // Try.
        In(Expr) * (T(Try) << End) * ExprDef[Expr] >>
          [](Match& _) { return Try << _(Expr); },

        // Prefix operator.
        In(Expr) * T(Op)[Op] * ExprDef[Expr] >>
          [](Match& _) {
            return DynamicCall
              << (Method << _(Expr) << (_(Op) / SymbolId) << (_(Op) / TypeArgs))
              << ExprSeq;
          },

        // Infix operator.
        In(Expr) * ExprDef[Lhs] * T(Op)[Op] * T(ExprSeq)[Rhs] >>
          [](Match& _) {
            return DynamicCall
              << (Method << _(Expr) << (_(Op) / SymbolId) << (_(Op) / TypeArgs))
              << _(Rhs);
          },

        In(Expr) * ExprDef[Lhs] * T(Op)[Op] * ExprDef[Rhs] >>
          [](Match& _) {
            return DynamicCall
              << (Method << _(Lhs) << (_(Op) / SymbolId) << (_(Op) / TypeArgs))
              << (ExprSeq << (Expr << _(Rhs)));
          },

        // Else.
        In(Expr) * AssignDef[Lhs] * (T(Else) << End) * AssignDef[Rhs] >>
          [](Match& _) { return Else << _(Lhs) << _(Rhs); },

        // Assignment is right-associative.
        In(Expr) * (T(Equals) << (AssignDef[Lhs] * AssignDef[Rhs])) *
            (T(Equals) << End) * AssignDef[Expr] >>
          [](Match& _) {
            return Equals << _(Lhs) << (Equals << _(Rhs) << _(Expr));
          },

        In(Expr) * AssignDef[Lhs] * (T(Equals) << End) * AssignDef[Rhs] >>
          [](Match& _) { return Equals << _(Lhs) << _(Rhs); },
      }};

    p.post([](auto top) {
      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Expr)
        {
          if (node->size() != 1)
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
