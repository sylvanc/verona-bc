#include "reifications.h"

namespace vc
{
  PathReification::PathReification(Reifications* rs, Subst& subst, Node path)
  : rs(rs), path(path), args(0), subst(subst), errors(false)
  {}

  PathReification::PathReification(
    Reifications* rs, Subst& subst, Node path, Node ref, Node args)
  : rs(rs),
    path(path),
    ref(ref),
    args(args->size()),
    subst(subst),
    errors(false)
  {}

  ReifyResult PathReification::run()
  {
    assert(path->in({TypeName, QName}));

    // All type arguments have already been resolved, as they are
    // deeper in the tree.
    auto elem = path->front();
    auto typeargs = elem / TypeArgs;
    auto defs = (elem / Ident)->lookup();
    bool found = false;

    // The initial lookup is a list of Use, ClassDef, TypeAlias,
    // TypeParam, ParamDef, Let, and Var. At most one will not be a
    // Use.
    for (auto& def : defs)
    {
      if (def == Use)
      {
        // Ignore `use` that don't syntactically precede the definition.
        if (!def->precedes(path))
          continue;

        // If this hasn't been reified, or fails, we're done.
        auto r = do_schedule(def, true);

        if (r == Fail)
          continue;

        if (r == Delay)
          return r;

        // Explore down this path.
        errors = false;
        r = do_path(0);
        errors = true;

        // Nothing found, try other paths.
        if (r == Fail)
          continue;

        // Come back to this later.
        if (r == Delay)
          return Delay;

        found = true;
        break;
      }
      else if (def == ClassDef)
      {
        // If this hasn't been reified, or fails, we're done.
        auto r = do_classdef(def, typeargs, true);

        if (r != Ok)
          return r;

        found = true;
        break;
      }
      else if (def == TypeAlias)
      {
        // If this hasn't been reified, or fails, we're done.
        auto r = do_typealias(def, typeargs, true);

        if (r != Ok)
          return r;

        found = true;
        break;
      }
      else if (def == TypeParam)
      {
        // Find the TypeParam in the substitution map.
        auto find = subst.find(def);

        if (find != subst.end())
        {
          assert(find->second == Type);
          assert(!find->second->empty());
          std::tie(curr_def, subst) = rs->get_def_subst(find->second->front());
        }
        else
        {
          path
            << (err("Depends on an unbound type parameter")
                << errloc(elem) << errmsg("Type parameter is defined here:")
                << errloc(def / Ident));
          return Fail;
        }

        found = true;
        break;
      }
      else
      {
        path
          << (err("Not a type") << errloc(elem) << errmsg("Defined here:")
                                << errloc(def / Ident));
        return Fail;
      }
    }

    if (!found)
    {
      curr_def = path->parent(ClassDef);
      return do_path(0);
    }

    return do_path(1);
  }

  ReifyResult PathReification::do_path(size_t i)
  {
    for (; i < path->size(); i++)
    {
      // Look down each path element, updating curr_def and subst.
      auto r = do_element(path->at(i), i == (path->size() - 1));

      // If this hasn't been reified, or fails, we're done.
      if (r != Ok)
        return r;
    }

    if (path == QName)
    {
      if (curr_def == ClassDef)
      {
        // If this is a QName and we have a class, do create sugar.
        auto create = QElement << (Ident ^ "create") << TypeArgs;
        auto r = do_element(create, true);

        if (r != Ok)
          return r;
      }

      if (curr_def == Function)
      {
        // Schedule the function for reification.
        auto&& [target_r, fresh] = rs->schedule(curr_def, subst, true);
        result = target_r.instance / Ident;
      }
      else
      {
        if (errors)
          path << (err("Expected a function") << errloc(path->back()));

        return Fail;
      }
    }
    else if (curr_def == ClassDef)
    {
      // Schedule the class for reification.
      auto&& [target_r, fresh] = rs->schedule(curr_def, subst, false);
      result = target_r.reified_name;
    }
    else
    {
      // Return the reified non-class result of a TypeAlias.
      result = curr_def;
    }

    return Ok;
  }

  ReifyResult PathReification::do_element(Node elem, bool last)
  {
    if (curr_def != ClassDef)
    {
      if (errors)
        path << (err("Path element doesn't follow a class") << errloc(elem));

      return Fail;
    }

    auto defs = curr_def->lookdown((elem / Ident)->location());
    auto typeargs = elem / TypeArgs;

    // Lookdown will be a ClassDef, a TypeAlias, or one or more Functions.
    bool found = false;

    for (auto& def : defs)
    {
      if (def == Function)
      {
        if ((path == QName) && last)
        {
          if (((def / Lhs) == ref->type()) && ((def / Params)->size() == args))
          {
            auto r = do_typeargs(def, typeargs, false);

            if (r != Ok)
              return r;

            curr_def = def;
            found = true;
            break;
          }
        }
        else
        {
          if (errors)
          {
            path
              << (err("Can't use a function as a type")
                  << errloc(elem) << errmsg("Function is defined here:")
                  << errloc(def / Ident));
          }

          return Fail;
        }
      }
      else if (def == ClassDef)
      {
        auto r = do_classdef(def, typeargs, false);

        if (r != Ok)
          return r;

        found = true;
        break;
      }
      else if (def == TypeAlias)
      {
        auto r = do_typealias(def, typeargs, false);

        if (r != Ok)
          return r;

        found = true;
        break;
      }
    }

    if (!found)
    {
      if (errors)
      {
        path
          << (err("Type not found") << errloc(elem) << errmsg("Expected here:")
                                    << errloc(curr_def / Ident));
      }

      return Fail;
    }

    return Ok;
  }

  ReifyResult
  PathReification::do_classdef(Node& def, Node& typeargs, bool scope)
  {
    auto r = do_typeargs(def, typeargs, scope);

    if (r != Ok)
      return r;

    // Schedule the class for reification.
    return do_schedule(def, false);
  }

  ReifyResult
  PathReification::do_typealias(Node& def, Node& typeargs, bool scope)
  {
    auto r = do_typeargs(def, typeargs, scope);

    if (r != Ok)
      return r;

    return do_schedule(def, true);
  }

  ReifyResult
  PathReification::do_typeargs(Node& def, Node& typeargs, bool scope)
  {
    // Get the default type arguments for the enclosing scope. Do this in
    // reverse order, so that when the default type arguments are reified,
    // they can only depend on syntactically previously defined type arguments.
    if (scope)
    {
      std::stack<Node> scopes;
      auto p = def->parent(ClassDef);

      while (p)
      {
        scopes.push(p);
        p = p->parent(ClassDef);
      }

      while (!scopes.empty())
      {
        p = scopes.top();
        scopes.pop();
        auto r = default_typeargs(p / TypeParams);

        if (r != Ok)
          return r;
      }
    }

    // Get the default type arguments for this def.
    return default_typeargs(def / TypeParams, typeargs);
  }

  ReifyResult PathReification::default_typeargs(Node typeparams, Node typeargs)
  {
    size_t num_typeargs = typeargs ? typeargs->size() : 0;

    if (num_typeargs > typeparams->size())
    {
      if (errors)
      {
        path
          << (err("Too many type arguments")
              << errloc(typeargs->parent())
              << errmsg("Type parameters are defined here:")
              << errloc(typeparams));
      }

      return Fail;
    }

    for (size_t i = 0; i < num_typeargs; i++)
    {
      if ((typeparams->at(i) == TypeParam) && (typeargs->at(i) != Type))
      {
        if (errors)
        {
          path
            << (err("Expected a type, got an expression")
                << errloc(typeargs->at(i))
                << errmsg("Type parameter is defined here:")
                << errloc(typeparams->at(i)));
        }

        return Fail;
      }

      if ((typeparams->at(i) == ValueParam) && (typeargs->at(i) != Expr))
      {
        if (errors)
        {
          path
            << (err("Expected an expression, got a type")
                << errloc(typeargs->at(i))
                << errmsg("Type parameter is defined here:")
                << errloc(typeparams->at(i)));
        }

        return Fail;
      }

      subst[typeparams->at(i)] = typeargs->at(i);
    }

    for (auto i = num_typeargs; i < typeparams->size(); i++)
    {
      Node dflt = typeparams->at(i) / Type;

      if (dflt->empty())
      {
        subst[typeparams->at(i)] = Dyn;
        return Ok;
      }

      // Reify the default type argument in place.
      auto r = Reification(rs, dflt, 0, subst);
      r.instantiate();
      r.run();

      if (r.status != Ok)
        return r.status;

      subst[typeparams->at(i)] = r.instance;
      r.instance->parent()->replace(r.instance);
    }

    return Ok;
  }

  ReifyResult PathReification::do_schedule(Node& def, bool enqueue)
  {
    auto&& [target_r, fresh] = rs->schedule(def, subst, enqueue);

    if (def == ClassDef)
    {
      curr_def = target_r.def;
      subst = target_r.subst_orig;
      return Ok;
    }

    if (target_r.status == Ok)
    {
      // This has already been resolved. If we can continue looking down
      // through this, get the definition site and the substitutions.
      if (def == TypeAlias)
      {
        std::tie(curr_def, subst) =
          rs->get_def_subst((target_r.instance / Type)->front());
      }
      else if (def == Use)
      {
        std::tie(curr_def, subst) =
          rs->get_def_subst(target_r.instance->front());
      }
      else
      {
        assert(false);
      }

      return Ok;
    }
    else if (fresh)
    {
      // This reification was just scheduled.
      return Delay;
    }
    else
    {
      // This reification was scheduled previously, but is delayed (circular
      // dependency) or failed.
      if (errors)
      {
        path
          << (err("This depends on something which in turn depends on this")
              << errloc(path) << errmsg("The dependency is defined here:")
              << errloc(def / Ident));
      }

      return Fail;
    }
  }
}
