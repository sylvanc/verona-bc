#include "../lang.h"

namespace vc
{
  PassDef dot()
  {
    PassDef p{
      "dot",
      wfPassDot,
      dir::topdown,
      {
        // Dot with arguments.
        In(Expr) * ValuePat[Lhs] * T(Dot)[Dot] * T(ExprSeq)[ExprSeq] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Expr)) << (_(Dot) / Ident)
                           << (_(Dot) / TypeArgs) << (Args << *_(ExprSeq));
          },

        // Dot without arguments.
        In(Expr) * ValuePat[Lhs] * T(Dot)[Dot] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Expr)) << (_(Dot) / Ident)
                           << (_(Dot) / TypeArgs) << Args;
          },
      }};

    return p;
  }
}
