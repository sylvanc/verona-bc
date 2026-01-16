#include "reifications.h"

namespace vc
{
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

    status = Delay;

    if (def == Symbol)
    {
      // Don't clone symbols.
      instance = def;
    }
    else
    {
      // Clone the original and put it in the parent.
      instance = clone(def);
      def->parent() << instance;
    }

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

    // Can't shape functions without implementations.
    if ((def == Function) && ((def->parent(ClassDef) / Shape) == Shape))
    {
      instance << err(instance, "Can't call a function prototype");
      status = Fail;
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
      {
        if (def->parent(ClassDef) == rs->builtin)
        {
          auto name = (def / Ident)->location().view();

          if (name == "array")
            instance / Ident = Array << clone(subst.begin()->second);
          else if (name == "ref")
            instance / Ident = Ref << clone(subst.begin()->second);
          else if (name == "cown")
            instance / Ident = Cown << clone(subst.begin()->second);
        }

        if ((instance / Ident) == Ident)
          instance / Ident = ClassId ^ id;
      }
      else if (def == TypeAlias)
      {
        instance / Ident = TypeId ^ id;
      }
      else if (def == Function)
      {
        instance / Ident = FunctionId ^ id;
      }

      // Remap cloned type parameters.
      auto num_tp = (instance / TypeParams)->size();

      for (size_t i = 0; i < num_tp; i++)
      {
        auto orig = (def / TypeParams)->at(i);
        subst[(instance / TypeParams)->at(i)] = std::move(subst[orig]);
        subst.erase(orig);
      }

      // Build the symbol table.
      for (auto& child : *instance)
      {
        if (!wfPassANF.build_st(child))
        {
          child << (err("Invalid definition") << errloc(child));
          status = Fail;
          return true;
        }
      }
    }

    return true;
  }

  void Reification::run()
  {
    delays = 0;
    literals.clear();

    instance->traverse(
      [&](Node& node) {
        // Don't try to reify nested elements.
        return (node == instance) ||
          !node->in({Error, ClassDef, TypeAlias, Use, Function});
      },
      [&](Node& node) {
        if (delays > 0)
          return;

        // TODO: type inference
        // RegisterRef, FieldRef, ArrayRef, ArrayRefConst
        // Load, Store
        // Lookup, CallDyn, When

        // Don't bother with bounds for intrinsics that are only used in
        // builtin, unless they may introduce a new reified type.
        if (node == TypeName)
          reify_typename(node);
        else if (node == ParamDef)
          bounds[node / Ident].assign(node / Type);
        else if (node->in({Return, Raise, Throw}))
          bounds[node / LocalId].use(node->parent(Function) / Type);
        else if (node == New)
          reify_new(node);
        else if (node->in({NewArray, NewArrayConst}))
          reify_newarray(node);
        else if (node == ConstStr)
          reify_string(node);
        else if (node == Copy)
          uf.unite(node / LocalId, node / Rhs);
        else if (node->in({Eq, Ne, Lt, Le, Gt, Ge, Typetest}))
          bounds[node / LocalId].assign(reify_builtin("bool"));
        else if (node->in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
          bounds[node / LocalId].assign(reify_builtin("f64"));
        else if (node == Bits)
          bounds[node / LocalId].assign(reify_builtin("u64"));
        else if (node == Len)
          bounds[node / LocalId].assign(reify_builtin("usize"));
        else if (node == MakePtr)
          bounds[node / LocalId].assign(reify_builtin("ptr"));
        else if (node == Convert)
          reify_convert(node);
        else if (node == Const)
          reify_const(node);
        else if (node == Call)
          reify_call(node);
        else if (node == FFI)
          reify_ffi(node);

        // TODO: from here
        else if (node == Lookup)
          reify_lookup(node);
        else if (node->in({FieldRef, ArrayRef, RegisterRef}))
          reify_ref(node);
        else if (node == When)
          reify_when(node);
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
        // Add any (already reified) methods that were requested.
        reify_lookups();
      }
      else if (instance == Function)
      {
        for (auto& node : literals)
          pick_literal_type(node);

        // Add this as a method to each requesting (already reified) class.
        std::for_each(wants_method.begin(), wants_method.end(), [&](auto& kv) {
          auto& cls = kv.first;
          auto& method_id = kv.second;
          (cls / ClassBody)
            << (Method << clone(method_id) << clone(instance / Ident));
        });
      }
    }
  }

  void Reification::want_method(Node cls, Node method_id)
  {
    assert(def == Function);
    auto find = wants_method.find(cls);

    if (find != wants_method.end())
    {
      assert(find->second->equals(method_id));
      return;
    }

    wants_method.emplace(cls, method_id);

    if (instance && (status == Ok))
      (cls / ClassBody)
        << (Method << clone(method_id) << clone(instance / Ident));
  }

  void Reification::reify_typename(Node node)
  {
    PathReification p(rs, subst, node);
    auto r = p.run();

    if (r == Ok)
    {
      node->parent()->replace(node, clone(p.result));

      if (def == Symbol)
      {
        // If an FFI symbol is being reified, reify all types mentioned in the
        // signature.
        auto& rf = rs->get_reification(p.result);
        rs->schedule(rf.def, rf.subst_orig, true);
      }
    }
    else if (r == Delay)
      delays++;
    else
      status = Fail;
  }

  void Reification::reify_new(Node node)
  {
    // Skip if this has already been reified.
    if ((node / NewArgs) == Args)
      return;

    auto& r = rs->get_reification((node / Type)->front());
    rs->schedule(r.def, r.subst_orig, true);

    if (r.status == Ok)
    {
      assert(r.instance == ClassDef);
      node / Type = r.reified_name;
      bounds[node / LocalId].assign(r.reified_name);

      auto body = r.instance / ClassBody;
      Node args = Args;
      std::map<Location, Node> argmap;

      for (auto& arg : *(node / NewArgs))
        argmap[(arg / Ident)->location()] = arg / Rhs;

      for (auto& f : *body)
      {
        if (f == FieldDef)
        {
          auto& name = (f / Ident)->location();
          auto find = argmap.find(name);

          if (find != argmap.end())
          {
            bounds[find->second].use(f / Type);
            args << (Arg << ArgCopy << find->second);
            argmap.erase(find);
          }
          else
          {
            auto msg = std::format("Missing initializer for `{}`", name.view());
            node << err(node, msg);
          }
        }
      }

      if (!argmap.empty())
      {
        for (auto& kv : argmap)
        {
          auto msg =
            std::format("Unknown field initializer `{}`", kv.first.view());
          node << err(node, msg);
        }
      }

      node / NewArgs = args;
    }
    else if (r.status == Delay)
    {
      delays++;
    }
    else
    {
      status = Fail;
    }
  }

  void Reification::reify_newarray(Node node)
  {
    Subst s;
    auto pdef = rs->builtin->lookdown(Location("array")).front();
    s[(pdef / TypeParams)->front()] = node / Type;
    auto&& [r, fresh] = rs->schedule(pdef, s, true);
    bounds[node / LocalId].assign(r.reified_name);
  }

  void Reification::reify_string(Node node)
  {
    Subst s;
    auto pdef = rs->builtin->lookdown(Location("array")).front();
    s[(pdef / TypeParams)->front()] = Type << clone(reify_builtin("u8"));
    auto&& [r, fresh] = rs->schedule(pdef, s, true);
    bounds[node / LocalId].assign(r.reified_name);
  }

  Node Reification::reify_builtin(const std::string& name)
  {
    auto pdef = rs->builtin->lookdown(Location(name)).front();
    auto&& [r, fresh] = rs->schedule(pdef, {}, true);
    return r.reified_name;
  }

  void Reification::reify_convert(Node node)
  {
    auto type = node / Type;
    Node r;

    if (type == None)
      r = reify_builtin("none");
    else if (type == Bool)
      r = reify_builtin("bool");
    else if (type == I8)
      r = reify_builtin("i8");
    else if (type == I16)
      r = reify_builtin("i16");
    else if (type == I32)
      r = reify_builtin("i32");
    else if (type == I64)
      r = reify_builtin("i64");
    else if (type == U8)
      r = reify_builtin("u8");
    else if (type == U16)
      r = reify_builtin("u16");
    else if (type == U32)
      r = reify_builtin("u32");
    else if (type == U64)
      r = reify_builtin("u64");
    else if (type == ILong)
      r = reify_builtin("ilong");
    else if (type == ULong)
      r = reify_builtin("ulong");
    else if (type == ISize)
      r = reify_builtin("isize");
    else if (type == USize)
      r = reify_builtin("usize");
    else if (type == F32)
      r = reify_builtin("f32");
    else if (type == F64)
      r = reify_builtin("f64");
    else
      assert(false);

    bounds[node / LocalId].assign(r);
  }

  void Reification::reify_const(Node node)
  {
    auto type = node / Type;

    if (type->in({U64, F64}))
      literals.push_back(node);
    else
      reify_convert(node);
  }

  void Reification::reify_call(Node node)
  {
    if ((node / QName) != QName)
      return;

    Node args = node / Args;
    PathReification p(rs, subst, node / QName, node / Lhs, args);
    auto r = p.run();

    if (r == Ok)
    {
      node / QName = clone(p.result / Ident);
      auto params = p.result / Params;
      assert(params->size() == args->size());

      for (size_t i = 0; i < args->size(); i++)
        bounds[args->at(i) / Rhs].use(params->at(i) / Type);

      bounds[node / LocalId].assign(p.result / Type);
    }
    else if (r == Delay)
      delays++;
    else
      status = Fail;
  }

  void Reification::reify_ffi(Node node)
  {
    auto sym = node / SymbolId;
    auto args = node / Args;
    auto body = node->parent(ClassBody);

    while (body)
    {
      for (auto& m : *body)
      {
        if (m != Lib)
          continue;

        for (auto& s : *(m / Symbols))
        {
          if (!(s / SymbolId)->equals(sym))
            continue;

          auto params = s / FFIParams;
          auto varargs = (s / Vararg) == Vararg;
          auto type = s / Type;

          if (!varargs && (args->size() != params->size()))
          {
            auto msg = std::format(
              "FFI call to `{}` expects {} arguments, got {}",
              sym->location().view(),
              params->size(),
              args->size());
            node << err(node, msg);
            status = Fail;
            return;
          }

          if (varargs && (args->size() < params->size()))
          {
            auto msg = std::format(
              "FFI call to `{}` expects at least {} arguments, got {}",
              sym->location().view(),
              params->size(),
              args->size());
            node << err(node, msg);
            status = Fail;
            return;
          }

          for (size_t i = 0; i < args->size(); i++)
            bounds[args->at(i) / Rhs].use(params->at(i));

          bounds[node / LocalId].assign(type);
          return;
        }
      }

      body = body->parent(ClassBody);
    }
  }

  void Reification::reify_lookup(Node node)
  {
    // TODO: method lookups
    // unions and intersections?
    // must exist on every type in a union, any type in an intersection
    // use subtyping with type variables?
    // auto receivers = uf.group(node / Rhs);
    // Nodes types;

    // for (auto& r : receivers)
    // {
    //   for (auto& t : bounds[r].lower)
    //     concrete_types(types, t);
    // }

    rs->add_lookup(node);
  }

  void Reification::reify_ref(Node /*node*/)
  {
    // TODO: not always ref[any]
    Subst s;
    auto pdef = rs->builtin->lookdown(Location("ref")).front();
    s[(pdef / TypeParams)->front()] = Type << clone(reify_builtin("any"));
    rs->schedule(pdef, s, true);
  }

  void Reification::reify_when(Node node)
  {
    Subst s;
    auto pdef = rs->builtin->lookdown(Location("cown")).front();
    s[(pdef / TypeParams)->front()] = node / Type;
    auto&& [r, fresh] = rs->schedule(pdef, s, true);

    if (r.status == Ok)
      bounds[node / LocalId].assign(r.reified_name);
    else if (r.status == Delay)
      delays++;
    else
      status = Fail;
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

  void Reification::concrete_types(Nodes& types, Node type)
  {
    type->traverse(
      [&](Node& n) { return n->in({Isect, Union, TypeNameReified}); },
      [&](Node& n) {
        if (n == TypeNameReified)
          types.push_back(n);

        return true;
      });
  }

  Nodes Reification::literal_types(Nodes& types, bool int_lit)
  {
    Nodes result;

    for (auto& t : types)
    {
      if (t != TypeNameReified)
        continue;

      auto& path = t->front();

      if (
        (path->size() != 2) || (path->front()->location().view() != "_builtin"))
        continue;

      auto cls = path->back()->location().view();

      if (
        (cls == "f32") || (cls == "f64") ||
        (int_lit &&
         ((cls == "i8") || (cls == "i16") || (cls == "i32") || (cls == "i64") ||
          (cls == "u8") || (cls == "u16") || (cls == "u32") || (cls == "u64") ||
          (cls == "ilong") || (cls == "ulong") || (cls == "isize") ||
          (cls == "usize"))))
        result.push_back(t);
    }

    return result;
  }

  void Reification::pick_literal_type(Node node)
  {
    if (!(node / Type)->in({U64, F64}))
      return;

    Nodes candidates;
    auto locals = uf.group(node / LocalId);

    for (auto& l : locals)
    {
      auto& b = bounds[l];
      assert(b.lower.empty());

      for (auto t : b.upper)
        concrete_types(candidates, t);
    }

    // TODO: just use the first one?
    // a concrete type that isn't from a union is "more precise".
    candidates = literal_types(candidates, (node / Type) == U64);
    Node t = candidates.empty() ? reify_builtin("u64") : candidates.front();
    bounds[node / LocalId].assign(t);

    // Set the literal type.
    auto& path = t->front();
    assert(
      (path->size() == 2) && (path->front()->location().view() == "_builtin"));
    auto cls = path->back()->location().view();

    if (cls == "i8")
      node / Type = I8;
    else if (cls == "i16")
      node / Type = I16;
    else if (cls == "i32")
      node / Type = I32;
    else if (cls == "i64")
      node / Type = I64;
    else if (cls == "u8")
      node / Type = U8;
    else if (cls == "u16")
      node / Type = U16;
    else if (cls == "u32")
      node / Type = U32;
    else if (cls == "u64")
      node / Type = U64;
    else if (cls == "ilong")
      node / Type = ILong;
    else if (cls == "ulong")
      node / Type = ULong;
    else if (cls == "isize")
      node / Type = ISize;
    else if (cls == "usize")
      node / Type = USize;
    else if (cls == "f32")
      node / Type = F32;
    else if (cls == "f64")
      node / Type = F64;
    else
      assert(false);
  }
}
