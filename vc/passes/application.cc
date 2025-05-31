#include "../lang.h"

namespace vc
{
  Node resolve(Node name);

  Node lookup(Node ident)
  {
    // Lookup returns a ClassDef, TypeAlias, TypeParam, ParamDef, Let, Var, or
    // an Error.
    assert(ident == Ident);
    auto defs = ident->lookup();

    // The initial lookup is list of Use, ClassDef, TypeAlias, TypeParam,
    // ParamDef, Let, and Var.
    for (auto& def : defs)
    {
      if (def == Use)
      {
        if (!def->precedes(ident))
          continue;

        // A TypeName resolve returns a ClassDef, TypeAlias, TypeParam, or an
        // Error.
        auto find = resolve(def / TypeName);

        if (find == Error)
          continue;

        // Lookdown returns a ClassDef, TypeAlias, TypeParam, FieldDef, or a
        // list of Functions.
        auto import_defs = find->lookdown(ident->location());

        if (import_defs.empty())
          continue;

        find = import_defs.front();

        if (find->in({FieldDef, Function}))
          continue;

        return find;
      }

      return def;
    }

    // Not found.
    return err(ident, "Identifier not found");
  }

  Node resolve(Node name)
  {
    if (name == Error)
      return name;

    // A TypeName resolve returns a ClassDef, TypeAlias, TypeParam, or an Error.
    // A QName resolve can also return a Function.
    assert(name->in({TypeName, QName}));

    // Resolve the leading name with a lookup.
    auto it = name->begin();
    auto ident = *it / Ident;
    auto find = lookup(ident);

    if (find == Error)
      return find;

    if (find->in({ParamDef, Let, Var}))
    {
      return err(
        ident, "Identifier is not a class, type alias, or type parameter");
    }

    assert(find->in({ClassDef, TypeAlias, TypeParam}));

    for (++it; it != name->end(); ++it)
    {
      if (find == Function)
        return err(ident, "Path is a function");

      // Lookdown returns a ClassDef, TypeAlias, TypeParam, FieldDef, or a list
      // of Functions.
      ident = *it / Ident;
      auto defs = find->lookdown(ident->location());

      if (defs.empty())
        return err(ident, "Path not found");

      find = defs.front();

      if (find == FieldDef)
        return err(ident, "Path is a field");
    }

    // If a TypeName is a function, we return an error.
    if ((name == TypeName) && (find == Function))
      return err(ident, "A type name can't be a function");

    return find;
  }

  PassDef application()
  {
    PassDef p{
      "application",
      wfPassApplication,
      dir::topdown,
      {
        // TypeName and QName resolition.
        T(TypeName, QName)[TypeName] >> [](Match& _) -> Node {
          auto find = resolve(_(TypeName));

          if (find == Error)
            return find;

          return NoChange;
        },

        // Ident resolution.
        In(Expr) * T(Ident)[Ident] >>
          [](Match& _) {
            auto ident = _(Ident);
            auto def = lookup(ident);

            if (def->in({ClassDef, TypeAlias, TypeParam}))
            {
              // Create sugar.
              return QName << (QElement << ident << TypeArgs)
                           << (QElement << (Ident ^ "create") << TypeArgs);
            }
            else if (def->in({ParamDef, Let}))
            {
              return RefLet << ident;
            }
            else if (def == Var)
            {
              return RefVar << ident;
            }

            assert(def == Error);
            return def;
          },

        // Application.
        In(Expr) * ApplyPat[Lhs] * ExprPat[Rhs] >>
          [](Match& _) { return Apply << _(Lhs) << _(Rhs); },

        // Extend an existing application.
        In(Expr) * T(Apply)[Lhs] * ExprPat[Rhs] >>
          [](Match& _) { return _(Lhs) << _(Rhs); },
      }};

    return p;
  }
}
