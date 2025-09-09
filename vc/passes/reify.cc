#include "../lang.h"
#include "../subtype.h"

#include <stack>

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

    // The result. This is a TypeNameReified (for a TypeName) or a FunctionId
    // (for a QName).
    Node result;

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

    // This is the reification index.
    size_t index;

    // This is a map of TypeParam to Type.
    Subst subst;

    // The type substitutions using the TypeParam nodes from def, not instance.
    Subst subst_orig;

    // This is a TypeNameReified for a ClassDef. Anything else is empty.
    Node reified_name;

    // This is a cloned instance of the original ClassDef, TypeAlias, or
    // Function (or a reference to the original if there are no substitutions).
    // This won't exist for a ClassDef unless we call `new` on that class.
    Node instance;

    // Mark this when the instance is finished reifying.
    ReifyResult status;
    size_t delays;

    // These are class reifications that want this reification as a method.
    std::vector<std::pair<Node, Node>> wants_method;

    Reification(Reifications* rs, Node def, size_t i, Subst& subst);

    bool instantiate();
    void run();
    void want_method(Node cls, Node method_id);

    void reify_typename(Node node);
    void reify_call(Node node);
    void reify_new(Node node);
    void reify_primitive(Node node);
    void reify_lookup(Node node);
    void reify_lookups();
  };

  // Lookup by name and arity and get a vector of invariant type arguments.
  using Lookups =
    std::map<Location, std::map<size_t, std::vector<std::pair<Node, Node>>>>;

  struct Reifications
  {
    // Keep the top of the AST to help resolve names.
    Node top;
    Node builtin;

    // A map of definition site to all reifications of that definition.
    NodeMap<std::vector<Reification>> map;

    // A work list of definition site and index into the reification list.
    std::stack<std::pair<Node, size_t>> wl;

    // LHS and RHS lookups.
    Lookups lhs_lookups;
    Lookups rhs_lookups;

    Reifications(Node top);
    void run();

    std::pair<Reification&, bool> schedule(Node def, Subst subst, bool enqueue);
    Reification& get_reification(Node type);
    std::pair<Node, Subst> get_def_subst(Node type);
    void add_lookup(Node lookup);
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

  Reification::Reification(Reifications* rs, Node def, size_t i, Subst& subst)
  : rs(rs),
    def(def),
    index(i),
    subst(subst),
    subst_orig(subst),
    status(Ok),
    delays(0)
  {
    if (def == ClassDef)
    {
      // Build a reified name that's the path plus an integer indicating which
      // index it is.
      Node path = TypePath << clone(def / Ident);
      auto p = def->parent(ClassDef);

      while (p)
      {
        path->push_front(clone(p / Ident));
        p = p->parent(ClassDef);
      }

      reified_name = TypeNameReified << path << (Int ^ std::to_string(i));
    }
  }

  bool Reification::instantiate()
  {
    // If we were already instantiated, return false.
    if (instance)
      return false;

    // Clone the original and put it in the parent.
    instance = clone(def);
    def->parent() << instance;

    // For classes, throw away all functions.
    if (def == ClassDef)
    {
      auto body = instance / ClassBody;
      Nodes remove;

      std::for_each(body->begin(), body->end(), [&](Node& n) {
        if (n == Function)
          remove.push_back(n);
      });

      for (auto& n : remove)
        body->replace(n);
    }

    if (def->in({ClassDef, TypeAlias, Function}))
    {
      // Create fully qualified name.
      auto id = std::string((def / Ident)->location().view());

      if (def == Function)
      {
        id = std::format(
          "{}.{}{}",
          id,
          (def / Params)->size(),
          (def / Lhs) == Lhs ? ".ref" : "");
      }

      id = std::format("{}[{}]", id, index);
      auto p = def->parent(ClassDef);

      while (p)
      {
        id = std::format("{}.{}", (p / Ident)->location().view(), id);
        p = p->parent(ClassDef);
      }

      if (def == ClassDef)
        instance / Ident = ClassId ^ id;
      else if (def == TypeAlias)
        instance / Ident = TypeId ^ id;
      else if (def == Function)
        instance / Ident = FunctionId ^ id;

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
        else if (node->in({Const, Convert}))
          reify_primitive(node);
        else if (node == Lookup)
          reify_lookup(node);
      });

    if ((status == Delay) && (delays == 0))
    {
      status = Ok;

      if ((instance == Use) && (instance->front() != TypeNameReified))
      {
        instance << (err("Doesn't resolve to a class") << errloc(instance));
        status = Fail;
      }
      else if (instance == ClassDef)
      {
        reify_lookups();
      }
      else if (instance == Function)
      {
        for (auto& [cls, method_id] : wants_method)
          (cls / ClassBody)
            << (Method << clone(method_id) << clone(instance / Ident));

        wants_method.clear();
      }
    }
  }

  void Reification::want_method(Node cls, Node method_id)
  {
    assert(def == Function);

    if (instance && (status == Ok))
      (cls / ClassBody)
        << (Method << clone(method_id) << clone(instance / Ident));
    else
      wants_method.emplace_back(cls, method_id);
  }

  void Reification::reify_typename(Node node)
  {
    PathReification p(rs, subst, node);
    auto r = p.run();

    if (r == Ok)
      node->parent()->replace(node, clone(p.result));
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
      node / QName = clone(p.result);
    else if (r == Delay)
      delays++;
    else
      status = Fail;
  }

  void Reification::reify_new(Node node)
  {
    auto& r = rs->get_reification((node / Type)->front());
    rs->schedule(r.def, r.subst_orig, true);
  }

  void Reification::reify_primitive(Node node)
  {
    auto type = node / Type;
    Node pdef;

    if (type == None)
      pdef = rs->builtin->lookdown(Location("none")).front();
    else if (type == Bool)
      pdef = rs->builtin->lookdown(Location("bool")).front();
    else if (type == I8)
      pdef = rs->builtin->lookdown(Location("i8")).front();
    else if (type == I16)
      pdef = rs->builtin->lookdown(Location("i16")).front();
    else if (type == I32)
      pdef = rs->builtin->lookdown(Location("i32")).front();
    else if (type == I64)
      pdef = rs->builtin->lookdown(Location("i64")).front();
    else if (type == U8)
      pdef = rs->builtin->lookdown(Location("u8")).front();
    else if (type == U16)
      pdef = rs->builtin->lookdown(Location("u16")).front();
    else if (type == U32)
      pdef = rs->builtin->lookdown(Location("u32")).front();
    else if (type == U64)
      pdef = rs->builtin->lookdown(Location("u64")).front();
    else if (type == ILong)
      pdef = rs->builtin->lookdown(Location("ilong")).front();
    else if (type == ULong)
      pdef = rs->builtin->lookdown(Location("ulong")).front();
    else if (type == ISize)
      pdef = rs->builtin->lookdown(Location("isize")).front();
    else if (type == USize)
      pdef = rs->builtin->lookdown(Location("usize")).front();
    else if (type == F32)
      pdef = rs->builtin->lookdown(Location("f32")).front();
    else if (type == F64)
      pdef = rs->builtin->lookdown(Location("f64")).front();
    else
      assert(false);

    rs->schedule(pdef, {}, true);
  }

  void Reification::reify_lookup(Node node)
  {
    rs->add_lookup(node);
  }

  void Reification::reify_lookups()
  {
    auto f = [&](auto& kv, Node side) {
      auto& name = kv.first;
      auto& map = kv.second;
      auto defs = def->lookdown(name);

      for (auto& func : defs)
      {
        if ((func != Function) || ((func / Lhs) != side->type()))
          continue;

        auto tp = func / TypeParams;
        auto tp_count = tp->size();
        auto arg_count = (func / Params)->size();

        std::for_each(map.begin(), map.end(), [&](auto& kv2) {
          auto arity = kv2.first;
          auto& v = kv2.second;

          if (arity != arg_count)
            return;

          std::for_each(v.begin(), v.end(), [&](auto& pair) {
            auto& ta = pair.first;
            auto& method_id = pair.second;

            if (ta->size() != tp_count)
              return;

            // Extend the class substitution map with the new type arguments.
            auto fsubst = subst_orig;

            for (size_t i = 0; i < tp->size(); i++)
              fsubst[tp->at(i)] = ta->at(i);

            // TODO: if this fails, don't report an error, and delete the
            // reification.
            auto&& [r, fresh] = rs->schedule(func, fsubst, true);
            r.want_method(instance, method_id);
          });
        });
      }
    };

    std::for_each(
      rs->lhs_lookups.begin(), rs->lhs_lookups.end(), [&](auto& kv) {
        return f(kv, Lhs);
      });

    std::for_each(
      rs->rhs_lookups.begin(), rs->rhs_lookups.end(), [&](auto& kv) {
        return f(kv, Rhs);
      });
  }

  Reifications::Reifications(Node top) : top(top)
  {
    // Get std::builtin.
    builtin = top->lookdown(Location("std"))
                .front()
                ->lookdown(Location("builtin"))
                .front();

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

      if (wl_def == Lookup)
      {
        // Lookups don't get delayed, so pop it immediately.
        wl.pop();

        // For each completed, reified class, check for a compatible function,
        // reify the function, and add the method.
        auto hand = (wl_def / Lhs)->type();
        auto name = (wl_def / Ident)->location();
        auto ta = wl_def / TypeArgs;
        auto ta_count = ta->size();
        auto arg_count = parse_int(wl_def / Int);
        auto method_id = wl_def->back();

        std::for_each(map.begin(), map.end(), [&](auto& kv) {
          if (kv.first != ClassDef)
            return;

          auto defs = kv.first->lookdown(name);
          Node func;

          for (auto& def : defs)
          {
            if (
              (def == Function) && ((def / Lhs) == hand) &&
              ((def / TypeParams)->size() == ta_count) &&
              ((def / Params)->size() == arg_count))
            {
              // We have a compatible function.
              func = def;
              break;
            }
          }

          if (!func)
            return;

          auto tp = func / TypeParams;

          for (auto& r : kv.second)
          {
            if ((r.status != Ok) || !r.instance)
              continue;

            // Extend the class substitution map with the new type arguments.
            auto subst = r.subst_orig;

            for (size_t i = 0; i < tp->size(); i++)
              subst[tp->at(i)] = ta->at(i);

            // TODO: if this fails, don't report an error, and delete the
            // reification.
            auto&& [r_func, fresh] = schedule(func, subst, true);
            r_func.want_method(r.instance, method_id);
          }
        });

        continue;
      }

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
    // Remove unassociated type parameters.
    if (def->in({ClassDef, TypeAlias, Function}))
    {
      Nodes remove;

      std::for_each(subst.begin(), subst.end(), [&](auto& kv) {
        auto& tp = kv.first;
        auto name = (tp / Ident)->location();
        auto found = false;
        auto p = def;

        while (p)
        {
          auto tps = p / TypeParams;

          if (std::any_of(
                tps->begin(), tps->end(), [&](auto& d) { return d == tp; }))
          {
            found = true;
            break;
          }

          p = p->parent(ClassDef);
        }

        if (!found)
          remove.push_back(tp);
      });

      for (auto& tp : remove)
        subst.erase(tp);
    }

    // Search for an existing reification with invariant substitutions.
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

    return map.at(def).at(parse_int(type / Int));
  }

  std::pair<Node, Subst> Reifications::get_def_subst(Node type)
  {
    // Return the type with no substitution map if this isn't a reified type
    // name.
    if (type != TypeNameReified)
      return {type, {}};

    auto& r = get_reification(type);
    return {r.def, r.subst_orig};
  }

  void Reifications::add_lookup(Node lookup)
  {
    assert(lookup == Lookup);
    auto& l = (lookup / Lhs) == Lhs ? lhs_lookups : rhs_lookups;
    auto& v = l[(lookup / Ident)->location()][parse_int(lookup / Int)];
    Node ta = lookup / TypeArgs;
    auto count = ta->size();
    size_t i = 0;
    Node method_id;

    for (; i < v.size(); i++)
    {
      auto& v_ta = v[i].first;
      method_id = v[i].second;

      if (v_ta->size() != count)
        continue;

      bool found = true;

      for (size_t j = 0; j < count; j++)
      {
        if (
          !subtype(v_ta->at(j), ta->at(j)) || !subtype(ta->at(j), v_ta->at(j)))
        {
          found = false;
          break;
        }
      }

      if (found)
        break;
    }

    if (i == v.size())
    {
      auto id = std::format(
        "{}.{}{}[{}]",
        (lookup / Ident)->location().view(),
        (lookup / Int)->location().view(),
        (lookup / Lhs) == Lhs ? ".ref" : "",
        i);

      method_id = MethodId ^ id;
      v.emplace_back(ta, method_id);
      wl.push({lookup, i});
    }

    lookup << clone(method_id);
  }

  PassDef reify()
  {
    PassDef p{
      "reify",
      wfPassReify,
      dir::bottomup | dir::once,
      {
        // Lift reified functions.
        T(Function)[Function] << (T(Lhs, Rhs) * T(FunctionId)) >>
          [](Match& _) -> Node {
          auto f = _(Function);
          return Lift << Top
                      << (Function << (f / Ident) << (f / Params) << (f / Type)
                                   << (f / Labels));
        },

        // Delete unreified functions.
        T(Function)[Function] << (T(Lhs, Rhs) * T(Ident, SymbolId)) >>
          [](Match& _) -> Node {
          if (_(Function)->get_contains_error())
            return NoChange;

          return {};
        },

        // Lift reified classes.
        T(ClassDef)[ClassDef] << T(ClassId) >> [](Match& _) -> Node {
          auto c = _(ClassDef);
          auto name = (c / Ident)->location().view();
          bool primitive = false;

          // Check for a primitive type.
          if (name.starts_with("std.builtin."))
          {
            primitive = true;

            if (name.ends_with("none[0]"))
              c / Ident = None;
            else if (name.ends_with("bool[0]"))
              c / Ident = Bool;
            else if (name.ends_with("i8[0]"))
              c / Ident = I8;
            else if (name.ends_with("i16[0]"))
              c / Ident = I16;
            else if (name.ends_with("i32[0]"))
              c / Ident = I32;
            else if (name.ends_with("i64[0]"))
              c / Ident = I64;
            else if (name.ends_with("u8[0]"))
              c / Ident = U8;
            else if (name.ends_with("u16[0]"))
              c / Ident = U16;
            else if (name.ends_with("u32[0]"))
              c / Ident = U32;
            else if (name.ends_with("u64[0]"))
              c / Ident = U64;
            else if (name.ends_with("ilong[0]"))
              c / Ident = ILong;
            else if (name.ends_with("ulong[0]"))
              c / Ident = ULong;
            else if (name.ends_with("isize[0]"))
              c / Ident = ISize;
            else if (name.ends_with("usize[0]"))
              c / Ident = USize;
            else if (name.ends_with("f32[0]"))
              c / Ident = F32;
            else if (name.ends_with("f64[0]"))
              c / Ident = F64;
            else
              primitive = false;
          }

          Node f;
          Node m = Methods;
          auto cls = (primitive ? Primitive : Class) << (c / Ident);

          if (!primitive)
          {
            f = Fields;
            cls << f;
          }

          cls << m;

          for (auto& n : *(c / ClassBody))
          {
            if (n == FieldDef)
            {
              if (primitive)
              {
                n->parent() << err(n, "Primitive types can't have fields.");
                return NoChange;
              }

              f << (Field << (FieldId ^ (n / Ident)) << (n / Type));
            }
            else if (n == Method)
            {
              m << n;
            }
          }

          return Lift << Top << cls;
        },

        // Delete use statements.
        T(Use)[Use] >> [](Match& _) -> Node {
          if (_(Use)->get_contains_error())
            return NoChange;

          return {};
        },

        // Delete type aliases.
        T(TypeAlias)[TypeAlias] >> [](Match& _) -> Node {
          if (_(TypeAlias)->get_contains_error())
            return NoChange;

          return {};
        },

        // Strip handedness from function calls.
        T(Call)
            << (T(LocalId)[Lhs] * T(Lhs, Rhs) * T(FunctionId)[FunctionId] *
                T(Args)[Args]) >>
          [](Match& _) -> Node {
          return Call << _(Lhs) << _(FunctionId) << _(Args);
        },

        // Use a MethodId in Lookup.
        T(Lookup)
            << (T(LocalId)[Lhs] * T(LocalId)[Rhs] * T(Lhs, Rhs) *
                T(Ident, SymbolId) * T(TypeArgs) * T(Int) *
                T(MethodId)[MethodId]) >>
          [](Match& _) -> Node {
          return Lookup << _(Lhs) << _(Rhs) << _(MethodId);
        },

        // Replace TypeNameReified with ClassId.
        T(TypeNameReified)[TypeNameReified] >> [](Match& _) -> Node {
          auto tn = _(TypeNameReified);
          std::string id;

          for (auto& ident : *(tn / TypePath))
            id = std::format(
              "{}{}{}", id, id.empty() ? "" : ".", ident->location().view());

          id = std::format("{}[{}]", id, parse_int(tn / Int));
          return ClassId ^ id;
        },
      }};

    p.pre([=](auto top) {
      Reifications reifications(top);
      reifications.run();
      return 0;
    });

    // Delete remaining ClassDef. Do this late, otherwise lifting doesn't work.
    p.post([=](auto top) {
      Nodes to_remove;

      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
        }
        else if (node == ClassDef)
        {
          if (!node->get_contains_error())
            to_remove.push_back(node);

          ok = false;
        }

        return ok;
      });

      for (auto& node : to_remove)
        node->parent()->replace(node);

      return 0;
    });

    return p;
  }
}
