#include "../lang.h"

namespace vc
{
  PassDef application()
  {
    PassDef p{
      "application",
      wfPassApplication,
      dir::topdown,
      {
        // C-style static call.
        In(Expr) * T(QName)[QName] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) { return Call << _(QName) << seq_to_args(_(ExprSeq)); },

        // C-style dynamic call on a 0-arg static call.
        In(Expr) * T(QName)[QName] *
            (T(Dot) << (T(Ident, SymbolId)[Ident] * T(TypeArgs)[TypeArgs])) *
            T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << (Expr << (Call << _(QName) << Args)) << _(Ident)
                           << _(TypeArgs) << seq_to_args(_(ExprSeq));
          },

        // C-style dynamic call.
        In(Expr) * LhsPat[Lhs] *
            (T(Dot) << (T(Ident, SymbolId)[Ident] * T(TypeArgs)[TypeArgs])) *
            T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << _(Ident) << _(TypeArgs)
                           << seq_to_args(_(ExprSeq));
          },

        // C-style apply sugar.
        In(Expr) * LhsPat[Lhs] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (Ident ^ "apply") << TypeArgs
                           << seq_to_args(_(ExprSeq));
          },

        // ML-style arguments.
        // Turn them into a C-style ExprSeq.
        In(Expr) * ((LhsPat / T(QName)) * ~T(Dot))[Lhs] *
            (RhsPat * RhsPat++)[Rhs] >>
          [](Match& _) { return Seq << _[Lhs] << (Rhs << _[Rhs]); },

        // Turn RHS elements in to Expr nodes.
        In(Rhs) * RhsPat[Rhs] >>
          [](Match& _) -> Node { return Expr << _[Rhs]; },

        // Turn an RHS with just Expr nodes into an ExprSeq.
        In(Expr) * (T(Rhs)[Rhs] << (T(Expr)++ * End)) >>
          [](Match& _) {
            return Seq << (ExprSeq << (Expr << (Tuple << *_[Rhs])));
          },

        // 0-arg calls.
        In(Expr) * ((T(QName) * ~T(Dot)) / (LhsPat * T(Dot)))[Expr] *
            --T(Rhs, ExprSeq) >>
          [](Match& _) { return Seq << _[Expr] << ExprSeq; },
      }};

    return p;
  }
}
