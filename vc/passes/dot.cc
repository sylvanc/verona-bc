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
        In(Expr) * ValuePat[Lhs] * T(Dot)[Dot] * T(Tuple)[Tuple] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(Dot) / Ident)
                           << (_(Dot) / TypeArgs) << (Args << *_(Tuple));
          },

        // Dot without arguments.
        In(Expr) * ValuePat[Lhs] * T(Dot)[Dot] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(Dot) / Ident)
                           << (_(Dot) / TypeArgs) << Args;
          },
      }};

    return p;
  }
}
