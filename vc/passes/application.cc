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
        // TypeName resolution.
        T(TypeName)[TypeName] >> [](Match& _) -> Node {
          auto def = resolve(_(TypeName));

          if (def == Error)
            return def;

          return NoChange;
        },

        // QName resolution.
        T(QName)[QName] >> [](Match& _) -> Node {
          auto def = resolve(_(QName));

          if (def == Error)
            return def;

          if (def->in({ClassDef, TypeAlias, TypeParam}))
            return _(QName) << (QElement << (Ident ^ "create") << TypeArgs);

          assert(def == Function);
          return NoChange;
        },

        // Ident resolution.
        In(Expr) * T(Ident)[Ident] >>
          [](Match& _) {
            auto ident = _(Ident);
            auto def = lookup(ident);

            if (!def)
            {
              // Not found, treat it as an operator.
              return Op << _(Ident) << TypeArgs;
            }
            else if (def->in({ClassDef, TypeAlias, TypeParam}))
            {
              // This will later be turned into create-sugar.
              return QName << (QElement << ident << TypeArgs);
            }
            else if (def->in({ParamDef, Let, Var}))
            {
              if (!def->precedes(ident))
                return err(ident, "Identifier used before definition");

              return LocalId ^ ident;
            }

            assert(def == Error);
            return def;
          },

        // Application.
        In(Expr) * ApplyLhsPat[Lhs] * ApplyRhsPat[Rhs] >>
          [](Match& _) {
            return Apply << (Expr << _(Lhs)) << (Expr << _(Rhs));
          },

        // Extend an existing application.
        In(Expr) * T(Apply)[Lhs] * ApplyRhsPat[Rhs] >>
          [](Match& _) { return _(Lhs) << (Expr << _(Rhs)); },
      }};

    return p;
  }
}
