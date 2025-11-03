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
        // Const, ConstStr, Convert
        // RegisterRef, FieldRef, ArrayRef, ArrayRefConst
        // Load, Store
        // Call, Lookup, CallDyn
        // Typetest, When, wfBinop, wfUnop

        if (node == TypeName)
          reify_typename(node);
        else if (node == Function)
          bounds.ret().use(node / Type);
        else if (node == ParamDef)
          bounds[node / Ident].assign(node / Type);
        else if (node->in({Return, Raise, Throw}))
          uf.unite_ret(node / LocalId);
        else if (node == Copy)
          uf.unite(node / LocalId, node / Rhs);
        else if (node == New)
          reify_new(node);
        else if (node->in({NewArray, NewArrayConst}))
          reify_newarray(node);
        else if (node == FFI)
          reify_ffi(node);
        else if (node->in({Const, Convert}))
          reify_primitive(node);
        else if (node->in({Eq, Ne, Lt, Le, Gt, Ge}))
          reify_bool(node);
        else if (node->in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
          reify_f64(node);
        else if (node == Call)
          reify_call(node);
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
        reify_lookups();
      }
      else if (instance == Function)
      {
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
        auto& rf = rs->get_reification(p.result);
        rs->schedule(rf.def, rf.subst_orig, true);
      }
    }
    else if (r == Delay)
    {
      delays++;
    }
    else
    {
      status = Fail;
    }
  }

  void Reification::reify_call(Node node)
  {
    if ((node / QName) != QName)
      return;

    PathReification p(rs, subst, node / QName, node / Lhs, node / Args);
    auto r = p.run();

    if (r == Ok)
      node / QName = clone(p.result);
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

    if (r.status == Ok)
      bounds[node / LocalId].assign(r.reified_name);
    else if (r.status == Delay)
      delays++;
    else
      status = Fail;
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

  void Reification::reify_bool(Node node)
  {
    auto pdef = rs->builtin->lookdown(Location("bool")).front();
    auto&& [r, fresh] = rs->schedule(pdef, {}, true);
    bounds[node / LocalId].assign(r.reified_name);
  }

  void Reification::reify_f64(Node node)
  {
    auto pdef = rs->builtin->lookdown(Location("f64")).front();
    auto&& [r, fresh] = rs->schedule(pdef, {}, true);
    bounds[node / LocalId].assign(r.reified_name);
  }

  void Reification::reify_lookup(Node node)
  {
    rs->add_lookup(node);
  }

  // void Reification::reify_fieldref(Node node)
  // {
  //   // TODO: only happens when the enclosing type is the arg
  // }

  void Reification::reify_ref(Node /*node*/)
  {
    // TODO: not always ref[any]
    Subst s;
    auto pdef = rs->builtin->lookdown(Location("ref")).front();
    s[(pdef / TypeParams)->front()] = Type
      << (TypeNameReified << (TypePath << (Ident ^ "builtin")
                                       << (Ident ^ "any"))
                          << (Int ^ "0"));
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
}
