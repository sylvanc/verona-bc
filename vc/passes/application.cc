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
        // C-style new.
        In(Expr) * (T(New) << End) * T(ExprSeq)[ExprSeq] >>
          [](Match& _) { return New << seq_to_args(_(ExprSeq)); },

        // C-style static call.
        In(Expr) * T(QName)[QName] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) { return Call << _(QName) << seq_to_args(_(ExprSeq)); },

        // C-style dynamic call.
        In(Expr) * T(Method)[Method] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << _(Method) << seq_to_args(_(ExprSeq));
          },

        // C-style apply sugar.
        In(Expr) * ApplyLhsPat[Expr] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << (Method << (Expr << _(Expr)) << (Ident ^ "apply")
                                      << TypeArgs)
                           << seq_to_args(_(ExprSeq));
          },

        // ML-style new.
        In(Expr) * (T(New) << End) * ApplyRhsPat++[Rhs] >>
          [](Match& _) {
            Node args = Args;

            for (auto& arg : _[Rhs])
              args << (Expr << arg);

            return New << args;
          },

        // ML-style static call.
        // This also turns a lone QName into a zero-argument call.
        In(Expr) * T(QName)[QName] * ApplyRhsPat++[Rhs] >>
          [](Match& _) {
            Node args = Args;

            for (auto& arg : _[Rhs])
              args << (Expr << arg);

            return Call << _(QName) << args;
          },

        // ML-style dynamic call.
        // This also turns a lone Method into a zero-argument call.
        In(Expr) * T(Method)[Method] * ApplyRhsPat++[Rhs] >>
          [](Match& _) {
            Node args = Args;

            for (auto& arg : _[Rhs])
              args << (Expr << arg);

            return CallDyn << _(Method) << args;
          },

        // ML-style apply sugar.
        In(Expr) * ApplyLhsPat[Lhs] * (ApplyRhsPat * ApplyRhsPat++)[Rhs] >>
          [](Match& _) {
            Node args = Args;

            for (auto& arg : _[Rhs])
              args << (Expr << arg);

            return CallDyn << (Method << (Expr << _(Lhs)) << (Ident ^ "apply")
                                      << TypeArgs)
                           << args;
          },
      }};

    return p;
  }
}
