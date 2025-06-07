#include "../lang.h"

namespace vc
{
  Node resolve(Node name);

  Node resolve_typealias(Node find)
  {
    while (find == TypeAlias)
    {
      find = find / Type;

      if (find->empty())
        return Error;

      find = find->front();

      if (find == TypeName)
        find = resolve(find);
      else
        find = Error;
    }

    return find;
  }

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
        find = resolve_typealias(find);

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
    return {};
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

    if (!find)
      return err(ident, "Identifier not found");

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

      find = resolve_typealias(find);

      if (find == Error)
        return err(ident, "Path is a complex type alias");

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

          // Error, create sugar, or function pointer.
          if (def == Error)
            return def;
          else if (def->in({ClassDef, TypeAlias, TypeParam}))
            return _(QName) << (QElement << (Ident ^ "create") << TypeArgs);
          else
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
              // Create sugar.
              return QName << (QElement << ident << TypeArgs)
                           << (QElement << (Ident ^ "create") << TypeArgs);
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
