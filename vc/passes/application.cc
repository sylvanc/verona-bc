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
        // Application.
        In(Expr) * ApplyDef[Lhs] * ExprDef[Rhs] >>
          [](Match& _) { return Apply << _(Lhs) << _(Rhs); },

        // Extend an existing application.
        In(Expr) * T(Apply)[Lhs] * ExprDef[Rhs] >>
          [](Match& _) { return _(Lhs) << _(Rhs); },
      }};

    return p;
  }
}
