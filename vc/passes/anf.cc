#include "../lang.h"

namespace vc
{
  const auto Liftable = LiteralPat /
    T(Tuple,
      // TODO: lift these or not?
      // Let,
      // Var,
      Lambda,
      QName,
      Method,
      Call,
      CallDyn,
      If,
      While,
      For,
      When,
      Else,
      Equals);

  PassDef anf()
  {
    PassDef p{
      "anf",
      wfPassANF,
      dir::topdown,
      {
        // L-values.
        In(Expr) * T(Equals) << (T(Expr)[Lhs] * T(Expr)[Rhs]) >>
          [](Match& _) {
            // TODO: experimenting
            return Equals << (Lhs << *_[Lhs]) << _(Rhs);
          },

        In(Lhs)++ * T(Expr)[Expr] >>
          [](Match& _) {
            // TODO: experimenting
            return Lhs << *_[Expr];
          },

        // Liftable expressions.
        In(Expr) * Liftable[Expr] >>
          [](Match& _) {
            auto id = _.fresh();
            return Seq << (Lift << Body << (Bind << (Ident ^ id) << _(Expr)))
                       << (RefLet << (Ident ^ id));
          },

        // Lift RefLet, RefVar.
        T(Expr) << (T(RefLet, RefVar)[Expr] * End) >>
          [](Match& _) { return _(Expr); },

        // Compact an ExprSeq with only one element.
        T(ExprSeq) << (Any[Expr] * End) >> [](Match& _) { return _(Expr); },

        // Discard leading RefLets in ExprSeq.
        In(ExprSeq) * T(RefLet) * ++Any >> [](Match&) -> Node { return {}; },

        // Discard non-trailing RefLets in Body.
        In(Body) * (T(Expr) << T(RefLet)) * ++Any >>
          [](Match&) -> Node { return {}; },
      }};

    return p;
  }
}
