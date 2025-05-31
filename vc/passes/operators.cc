#include "../lang.h"

namespace vc
{
  Node make_args(NodeRange r)
  {
    Node seq = Args;

    for (auto& n : r)
      seq << (Expr << n);

    return seq;
  }

  PassDef operators()
  {
    PassDef p{
      "operators",
      wfPassOperators,
      dir::topdown,
      {
        // Ref.
        In(Expr) * (T(Ref) << End) * ExprPat[Expr] >>
          [](Match& _) { return Ref << _(Expr); },

        // Try.
        In(Expr) * (T(Try) << End) * ExprPat[Expr] >>
          [](Match& _) { return Try << _(Expr); },

        // Prefix operator.
        In(Expr) * T(Op)[Op] * ExprPat[Expr] >>
          [](Match& _) {
            return DynamicCall
              << (Method << (Expr << _(Expr)) << (_(Op) / SymbolId)
                         << (_(Op) / TypeArgs))
              << Args;
          },

        // Infix operator.
        In(Expr) * ExprPat[Expr] * T(Op)[Op] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return DynamicCall
              << (Method << (Expr << _(Expr)) << (_(Op) / SymbolId)
                         << (_(Op) / TypeArgs))
              << seq_to_args(_(ExprSeq));
          },

        In(Expr) * ExprPat[Lhs] * T(Op)[Op] * ExprPat[Rhs] >>
          [](Match& _) {
            return DynamicCall
              << (Method << (Expr << _(Lhs)) << (_(Op) / SymbolId)
                         << (_(Op) / TypeArgs))
              << (Args << (Expr << _(Rhs)));
          },

        // Else.
        In(Expr) * AssignPat[Lhs] * (T(Else) << End) * AssignPat[Rhs] >>
          [](Match& _) { return Else << _(Lhs) << _(Rhs); },

        // Assignment is right-associative.
        In(Expr) * (T(Equals) << (AssignPat[Lhs] * AssignPat[Rhs])) *
            (T(Equals) << End) * AssignPat[Expr] >>
          [](Match& _) {
            return Equals << _(Lhs) << (Equals << _(Rhs) << _(Expr));
          },

        In(Expr) * AssignPat[Lhs] * (T(Equals) << End) * AssignPat[Rhs] >>
          [](Match& _) { return Equals << _(Lhs) << _(Rhs); },

        // Unpack apply: static call.
        T(Apply) << (T(QName)[QName] * ExprPat++[Expr]) >>
          [](Match& _) { return StaticCall << _(QName) << make_args(_[Expr]); },

        // Unpack apply: dynamic call.
        T(Apply) << (T(Method)[Method] * ExprPat++[Expr]) >>
          [](Match& _) {
            return DynamicCall << _(Method) << make_args(_[Expr]);
          },

        // Unpack apply: insert call to `apply`.
        T(Apply) << (ExprPat[Lhs] * ExprPat++[Expr]) >>
          [](Match& _) {
            return DynamicCall
              << (Method << (Expr << _(Lhs)) << (Ident ^ "apply") << TypeArgs)
              << make_args(_[Expr]);
          },
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
