#include "../lang.h"
#include "../subtype.h"

#include <stack>
#include <vbcc/from_chars.h>

namespace vc
{
  struct Reifications;
  struct Reification;
  using Subst = NodeMap<Node>;

  enum ReifyResult
  {
    Ok,
    Delay,
    Fail,
  };

  struct PathReification
  {
    // Allows scheduling new reifications, and looking up reified names.
    Reifications* rs;

    // The TypeName or QName being reified.
    Node path;

    // For a QName, the LHS/RHS and arity.
    Node ref;
    size_t args;

    // The current definition the path is relative to.
    Node curr_def;

    // The current type substitutions.
    Subst subst;

    // True if we're emitting errors, false if not. This is false when exploring
    // a `use` include to see if there's a definition available.
    bool errors;

    PathReification(Reifications* rs, Subst& subst, Node path);
    PathReification(
      Reifications* rs, Subst& subst, Node path, Node ref, Node args);

    ReifyResult run();
    ReifyResult do_path(size_t i);
    ReifyResult do_element(Node elem, bool last);
    ReifyResult do_classdef(Node& def, Node& typeargs, bool scope);
    ReifyResult do_typealias(Node& def, Node& typeargs, bool scope);
    ReifyResult do_typeargs(Node& def, Node& typeargs, bool scope);
    ReifyResult default_typeargs(Node typeparams, Node typeargs = {});
    ReifyResult do_schedule(Node& def, bool enqueue);
  };

  struct Reification
  {
    // Keep a pointer to all Reifications. This is used to schedule new
    // reifications and to look up reified names.
    Reifications* rs;

    // This is the original definition.
    Node def;

    // This is a map of TypeParam to Type.
    Subst subst;

    // This is a TypeNameReified or QNameReified representing this reification.
    Node name;

    // This is a cloned instance of the original ClassDef, TypeAlias, or
    // Function (or a reference to the original if there are no substitutions).
    // This won't exist for a ClassDef unless we call `new` on that class.
    Node instance;

    // Mark this when the instance is finished reifying.
    ReifyResult status;
    size_t delays;

    Reification(Reifications* rs, Node def, size_t i, Subst& subst);

    bool instantiate();
    void run();

    void reify_typename(Node node);
    void reify_call(Node node);
    void reify_new(Node node);
  };

  struct Reifications
  {
    // Keep the top of the AST to help resolve names.
    Node top;

    // A map of definition site to all reifications of that definition.
    NodeMap<std::vector<Reification>> map;

    // A work list of definition site and index into the reification list.
    std::stack<std::pair<Node, size_t>> wl;

    Reifications(Node top);

    void run();
    std::pair<Reification&, bool> schedule(Node def, Subst subst, bool enqueue);
    Reification& get_reification(Node type);
    std::pair<Node, Subst> get_def_subst(Node type);
  };

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

        if (r != Ok)
          return r;

        // Explore down this path.
        errors = false;
        r = do_path(0);

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
      path << (err("Type not found") << errloc(elem));
      return Fail;
    }

    errors = true;
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
        curr_def = clone(target_r.name);
      }

      if (curr_def != QNameReified)
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
      curr_def = clone(target_r.name);
    }
    else
    {
      // Return the reified non-class result of a TypeAlias.
      curr_def = clone(curr_def);
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

    r = do_schedule(def, true);

    if (r == Fail)
    {
      if (errors)
      {
        path
          << (err("This depends on a type alias which in turn depends on this")
              << errloc(typeargs->parent())
              << errmsg("The type alias is defined here:")
              << errloc(def / Ident));
      }
    }

    return r;
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
    auto r = default_typeargs(def / TypeParams, typeargs);

    if (r != Ok)
      return r;

    curr_def = def;
    return Ok;
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
        if (errors)
        {
          path
            << (err("Too few type arguments")
                << errloc(typeargs)
                << errmsg("Type parameters are defined here:")
                << errloc(typeparams));
        }

        return Fail;
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

    if (target_r.status == Ok)
    {
      // This has already been resolved. If we can continue looking down
      // through this, get the definition site and the substitutions.
      if (def == ClassDef)
      {
        curr_def = target_r.def;
        subst = target_r.subst;
      }
      else if (def == TypeAlias)
      {
        std::tie(curr_def, subst) = rs->get_def_subst(target_r.instance / Type);
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
      return Fail;
    }
  }

  Reification::Reification(Reifications* rs, Node def, size_t i, Subst& subst)
  : rs(rs), def(def), subst(subst), status(Ok), delays(0)
  {
    if (def->in({ClassDef, TypeAlias, Function}))
    {
      // Build a reified name that's the path plus an integer indicating which
      // index it is.
      Node path = TypePath << clone(def / Ident);
      name = (def == Function) ? QNameReified : TypeNameReified;
      name << path << (Int ^ std::to_string(i));
      auto p = def->parent(ClassDef);

      while (p)
      {
        path->push_front(clone(p / Ident));
        p = p->parent(ClassDef);
      }
    }
  }

  bool Reification::instantiate()
  {
    // If we were already instantiated, return false.
    if (instance)
      return false;

    // If no substitutions are needed, we can just use the original.
    if (subst.empty())
    {
      instance = def;
      return true;
    }

    // Clone the original and put it in the parent.
    instance = clone(def);
    def->parent() << instance;

    // A `use` has no identifier. Don't build the symbol table for it, to
    // avoid getting repetitive includes.
    if (instance->in({ClassDef, TypeAlias, Function}))
    {
      // Create name based on the reification index.
      instance / Ident = Ident ^
        (std::format(
          "{}[{}]",
          (name / TypePath)->back()->location().view(),
          (name / Int)->location().view()));

      // Remap cloned type parameters.
      auto num_tp = (instance / TypeParams)->size();

      for (size_t i = 0; i < num_tp; i++)
      {
        auto orig = (def / TypeParams)->at(i);
        subst[(instance / TypeParams)->at(i)] = std::move(subst[orig]);
        subst.erase(orig);
      }

      // Build the symbol table.
      wfPassANF.build_st(instance);
    }

    return true;
  }

  void Reification::run()
  {
    delays = 0;

    instance->traverse(
      [&](Node& node) {
        // Don't try to reify nested elements.
        return (node == instance) ||
          !node->in({Error, ClassDef, TypeAlias, Use, Function});
      },
      [&](Node& node) {
        if (delays > 0)
          return;

        if (node == TypeName)
          reify_typename(node);
        else if (node == Call)
          reify_call(node);
        else if (node == New)
          reify_new(node);
      });

    if ((status == Delay) && (delays == 0))
    {
      status = Ok;

      if ((instance == Use) && (instance->front() != TypeNameReified))
      {
        instance << (err("Doesn't resolve to a class") << errloc(instance));
        status = Fail;
      }
    }
  }

  void Reification::reify_typename(Node node)
  {
    PathReification p(rs, subst, node);
    auto r = p.run();

    if (r == Ok)
      node->parent()->replace(node, p.curr_def);
    else if (r == Delay)
      delays++;
    else
      status = Fail;
  }

  void Reification::reify_call(Node node)
  {
    PathReification p(rs, subst, node / QName, node / Lhs, node / Args);
    auto r = p.run();

    if (r == Ok)
      node / QName = p.curr_def;
    else if (r == Delay)
      delays++;
    else
      status = Fail;
  }

  void Reification::reify_new(Node node)
  {
    auto& r = rs->get_reification((node / Type)->front());
    rs->schedule(r.def, r.subst, true);
  }

  Reifications::Reifications(Node top) : top(top)
  {
    // Assume the main module is the first one.
    auto main_module = top->front();
    assert(main_module == ClassDef);
    assert((main_module / TypeParams)->empty());

    // Look for an LHS function with no parameters.
    auto defs = main_module->lookdown(Location("main"));
    Node main;

    for (auto& def : defs)
    {
      if (
        (def != Function) || ((def / Lhs) != Rhs) || (!(def / Params)->empty()))
        continue;

      main = def;
      break;
    }

    if (!main)
    {
      top->replace(main_module, err(main_module, "No main function."));
      return;
    }

    if (!(main / TypeParams)->empty())
    {
      main_module->replace(
        main, err(main, "Main function can't have type parameters."));
      return;
    }

    schedule(main, {}, true);
  }

  void Reifications::run()
  {
    while (!wl.empty())
    {
      auto [wl_def, wl_i] = wl.top();
      auto& r = map[wl_def][wl_i];

      if (r.status != Delay)
      {
        // Don't pop at the end, since we may have scheduled more work on the
        // stack that needs to be processed.
        wl.pop();
        continue;
      }

      r.run();
    }
  }

  std::pair<Reification&, bool>
  Reifications::schedule(Node def, Subst subst, bool enqueue)
  {
    auto& r = map[def];
    size_t i = 0;
    bool fresh = false;

    for (; i < r.size(); i++)
    {
      // If we have an existing reification with invariant substitutions,
      // return that.
      if (std::equal(
            r[i].subst.begin(),
            r[i].subst.end(),
            subst.begin(),
            subst.end(),
            [&](auto& lhs, auto& rhs) {
              assert(lhs.first == rhs.first);
              return subtype(lhs.second, rhs.second) &&
                subtype(rhs.second, lhs.second);
            }))
      {
        break;
      }
    }

    // Create a new reification with these substitutions.
    if (i == r.size())
    {
      r.emplace_back(this, def, i, subst);
      fresh = true;
    }

    if (enqueue && r[i].instantiate())
    {
      r[i].status = Delay;
      wl.push({def, i});
    }

    return {r[i], fresh};
  };

  Reification& Reifications::get_reification(Node type)
  {
    // Look down a reified type name and return the reification.
    assert(type == TypeNameReified);
    Node def = top;

    for (auto& ident : *(type / TypePath))
    {
      auto defs = def->lookdown(ident->location());
      assert(defs.size() == 1);
      def = defs.front();
    }

    auto view = (type / Int)->location().view();
    auto first = view.data();
    auto last = first + view.size();

    size_t i;
    std::from_chars(first, last, i, 10);
    return map.at(def).at(i);
  }

  std::pair<Node, Subst> Reifications::get_def_subst(Node type)
  {
    // Return the type with no substitution map if this isn't a reified type
    // name.
    if (type != TypeNameReified)
      return {type, {}};

    auto& r = get_reification(type);
    return {r.def, r.subst};
  }

  PassDef reify()
  {
    PassDef p{"reify", wfPassReify, dir::bottomup | dir::once, {}};

    p.pre([=](auto top) {
      Reifications reifications(top);
      reifications.run();
      return 0;
    });

    return p;
  }
}
