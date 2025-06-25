#include "lang.h"

namespace vc
{
  Node seq_to_args(Node seq)
  {
    assert(seq == ExprSeq);

    // Empty parentheses.
    if (seq->empty())
      return Args;

    if (seq->size() == 1)
    {
      auto expr = seq->front();

      // Turn a comma-delimited list into separate Args.
      if (expr == List)
        return Args << *expr;

      if (expr->size() == 1)
      {
        auto e = expr->front();

        // Turn a tuple into separate Args.
        if (e == Tuple)
          return Args << *e;
      }
    }

    // Treat this as a single argument.
    return Args << (Expr << seq);
  }

  Node make_typeargs(Node typeparams)
  {
    Node ta = TypeArgs;

    for (auto& tp : *typeparams)
    {
      ta
        << (Type
            << (TypeName << (TypeElement << clone(tp / Ident) << TypeArgs)));
    }

    return ta;
  }

  Node make_selftype(Node node)
  {
    auto cls = node->parent(ClassDef);
    auto tps = cls / TypeParams;
    return Type
      << (TypeName
          << (TypeElement << clone(cls / Ident) << make_typeargs(tps)));
  }

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

        // Lookdown returns a ClassDef, TypeAlias, TypeParam, or a list of
        // Functions.
        auto import_defs = find->lookdown(ident->location());

        if (import_defs.empty())
          continue;

        find = import_defs.front();

        if (find == Function)
          continue;

        return find;
      }

      return def;
    }

    // Not found.
    return {};
  }

  Nodes resolve_list(Node name)
  {
    if (name == Error)
      return {name};

    // A TypeName resolve returns a ClassDef, TypeAlias, TypeParam, or an Error.
    // A QName resolve can also return a Function.
    assert(name->in({TypeName, QName}));

    // Resolve the leading name with a lookup.
    auto it = name->begin();
    auto ident = *it / Ident;
    auto find = lookup(ident);

    if (!find)
      return {err(ident, "Identifier not found")};

    if (find == Error)
      return {find};

    if (find->in({ParamDef, Let, Var}))
    {
      return {
        err(ident, "Identifier is not a class, type alias, or type parameter")};
    }

    assert(find->in({ClassDef, TypeAlias, TypeParam}));

    for (++it; it != name->end(); ++it)
    {
      // If it's a type alias, look through it.
      find = resolve_typealias(find);

      if (find == Error)
        return {err(ident, "Path is a complex type alias")};

      // Lookdown returns a ClassDef, TypeAlias, TypeParam, or a list of
      // Functions.
      ident = *it / Ident;
      auto defs = find->lookdown(ident->location());

      if (defs.empty())
        return {err(ident, "Path not found")};

      find = defs.front();

      if (find == Function)
      {
        if (it + 1 != name->end())
          return {err(ident, "Path is a function")};

        // If a TypeName is a function, we return an error.
        if (name == TypeName)
          return {err(ident, "A type name can't be a function")};

        // If the last element is a function, return all definitions.
        return defs;
      }
    }

    return {find};
  }

  Node resolve(Node name)
  {
    return resolve_list(name).front();
  }

  Node resolve_qname(Node qname, Node side, size_t arity)
  {
    auto defs = resolve_list(qname);

    for (auto& def : defs)
    {
      if (def == Error)
        return def;

      if (
        (def == Function) && ((def / Lhs) == side->type()) &&
        ((def / Params)->size() == arity))
        return def;
    }

    return err(qname, "Function not found");
  }
}
