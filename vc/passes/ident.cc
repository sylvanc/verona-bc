#include "../lang.h"

namespace vc
{
  PassDef ident()
  {
    PassDef p{
      "ident",
      wfPassIdent,
      dir::bottomup | dir::once,
      {
        // Ident resolution.
        In(Expr) * T(Ident)[Ident] >> [](Match& _) -> Node {
          auto ident = _(Ident);

          if (ident->location().view() == "ref")
            return Ref;

          auto defs = ident->lookup();

          for (auto& def : defs)
          {
            if (def->in({ParamDef, Let, Var}))
            {
              if (!def->precedes(ident))
                return err(ident, "Identifier used before definition");

              return LocalId ^ ident;
            }
          }

          // Not a local, treat it as a static call.
          return QName << (QElement << ident << TypeArgs);
        },
      }};

    return p;
  }
}
