#include "lang.h"

namespace vc
{
  namespace
  {
    Node builtin_typename(std::string_view name, Node arg = {})
    {
      Node elem = NameElement << (Ident ^ std::string(name)) << TypeArgs;

      if (arg)
        elem / TypeArgs << clone(arg);

      return TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                      << elem;
    }
  }

  Node make_type(NodeRange r)
  {
    return Type << (r || TypeVar);
  }

  Node make_typeargs(Node typeparams)
  {
    Node ta = TypeArgs;

    for (auto& tp : *typeparams)
    {
      ta
        << (Type
            << (TypeName << (NameElement << clone(tp / Ident) << TypeArgs)));
    }

    return ta;
  }

  Nodes scope_path(Node node)
  {
    Nodes path;
    auto s = node;

    while (s && (s != Top))
    {
      path.push_back(s);
      s = s->parent({Top, ClassDef, TypeAlias, Function});
    }

    std::reverse(path.begin(), path.end());
    return path;
  }

  Node find_def_from(Node def, const Node& name, NodeIt it, NodeIt end)
  {
    if (it == end)
      return def;

    auto& elem = *it;
    assert(elem == NameElement);
    auto defs = def->look((elem / Ident)->location());

    for (auto& d : defs)
    {
      auto result = find_def_from(d, name, it + 1, end);

      if (result)
        return result;
    }

    return {};
  }

  Node find_def(Node top, const Node& name)
  {
    assert(name->in({FuncName, TypeName}));
    return find_def_from(top, name, name->begin(), name->end());
  }

  Node find_func_def(Node top, const Node& funcname, size_t arity, Node hand)
  {
    assert(funcname == FuncName);
    Node def = top;

    for (auto it = funcname->begin(); it != funcname->end(); ++it)
    {
      auto& elem = *it;
      assert(elem == NameElement);
      auto defs = def->look((elem / Ident)->location());

      if (defs.empty())
        return {};

      bool is_last = (it + 1 == funcname->end());

      if (is_last)
      {
        for (auto& d : defs)
        {
          if (d != Function)
            continue;

          auto def_hand = (d / Lhs)->type();
          if (
            hand && def_hand != hand->type() &&
            !(def_hand == Once && hand->type() == Rhs))
            continue;

          if ((d / Params)->size() != arity)
            continue;

          return d;
        }

        return {};
      }
      else
      {
        def = defs.front();

        if (def == TypeParam)
          return {};
      }
    }

    return {};
  }

  Node fq_typeparam(const Nodes& path, Node tp)
  {
    Node tn = TypeName;

    for (auto& s : path)
      tn << (NameElement << clone(s / Ident) << TypeArgs);

    tn << (NameElement << clone(tp / Ident) << TypeArgs);
    return tn;
  }

  Node fq_typeargs(const Nodes& path, Node tps)
  {
    Node ta = TypeArgs;

    for (auto& tp : *tps)
      ta << (Type << fq_typeparam(path, tp));

    return ta;
  }

  Node make_selftype(Node node, bool fq)
  {
    auto cls = node->parent(ClassDef);
    auto tps = cls / TypeParams;
    auto path = scope_path(cls);
    auto ta = fq ? fq_typeargs(path, tps) : make_typeargs(tps);

    Node tn = TypeName;

    for (auto& s : path)
    {
      if (s == cls)
        tn << (NameElement << clone(cls / Ident) << ta);
      else
        tn << (NameElement << clone(s / Ident) << TypeArgs);
    }

    return Type << tn;
  }

  Node type_any()
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "any") << TypeArgs));
  }

  Node type_nomatch()
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "nomatch") << TypeArgs));
  }

  Node ffi_struct_result_type()
  {
    Node usize = Type << builtin_typename("usize");
    Node u8 = Type << builtin_typename("u8");
    return TupleType << clone(usize->front())
                     << builtin_typename("array", usize)
                     << builtin_typename("array", u8);
  }

  Node make_nomatch(Node localid)
  {
    assert(localid == LocalId);
    return Call << (LocalId ^ localid) << Rhs
                << (FuncName
                    << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                    << (NameElement << (Ident ^ "nomatch") << TypeArgs)
                    << (NameElement << (Ident ^ "create") << TypeArgs))
                << Args;
  }

  std::vector<FreeTP> collect_free_typeparams(Node node)
  {
    std::vector<FreeTP> free_tps;
    std::set<std::string> seen;
    auto scope = node->parent({ClassDef, Function});

    while (scope)
    {
      auto sp = scope_path(scope);

      for (auto& tp : *(scope / TypeParams))
      {
        auto name = std::string((tp / Ident)->location().view());

        if (seen.insert(name).second)
          free_tps.push_back({name, sp});
      }

      scope = scope->parent({ClassDef, Function});
    }

    return free_tps;
  }

  // Check if a TypeName is a FQ reference to one of the free type params.
  // Returns the index into free_tps, or -1.
  static int match_free_tp(const Node& tn, const std::vector<FreeTP>& free_tps)
  {
    for (size_t i = 0; i < free_tps.size(); i++)
    {
      auto& ftp = free_tps[i];

      if (tn->size() != ftp.path.size() + 1)
        continue;

      bool match = true;

      for (size_t j = 0; j < ftp.path.size(); j++)
      {
        if (
          (tn->at(j) / Ident)->location().view() !=
          (ftp.path[j] / Ident)->location().view())
        {
          match = false;
          break;
        }
      }

      if (
        match &&
        ((tn->at(ftp.path.size()) / Ident)->location().view() == ftp.name))
        return static_cast<int>(i);
    }

    return -1;
  }

  // Build a new TypeName for a free type param redirected to the new class.
  static Node redirect_tp(
    const std::vector<FreeTP>& free_tps,
    size_t idx,
    const Nodes& cls_path,
    Location new_class_id)
  {
    Node new_tn = TypeName;

    for (auto& s : cls_path)
      new_tn << (NameElement << clone(s / Ident) << TypeArgs);

    new_tn << (NameElement << (Ident ^ new_class_id) << TypeArgs);
    new_tn << (NameElement << (Ident ^ free_tps[idx].name) << TypeArgs);
    return new_tn;
  }

  void rewrite_typeparam_refs(
    Node subtree,
    const std::vector<FreeTP>& free_tps,
    const Nodes& cls_path,
    Location new_class_id)
  {
    std::vector<std::pair<Node, size_t>> refs;

    subtree->traverse([&](auto node) {
      if (node == TypeName)
      {
        int idx = match_free_tp(node, free_tps);

        if (idx >= 0)
          refs.push_back({node, static_cast<size_t>(idx)});
      }

      return true;
    });

    for (auto& [old_tn, idx] : refs)
    {
      auto new_tn = redirect_tp(free_tps, idx, cls_path, new_class_id);
      old_tn->parent()->replace(old_tn, new_tn);
    }
  }

  AnonClass make_anon_class(
    Location id,
    Node context_node,
    const std::vector<FreeTP>& free_tps,
    std::vector<AnonClassField>& fields,
    Node apply_params,
    Node apply_ret_type,
    Node apply_body,
    bool is_block)
  {
    auto enclosing_cls = context_node->parent(ClassDef);
    assert(enclosing_cls);
    auto cls_path = scope_path(enclosing_cls);
    auto cls_ta = fq_typeargs(cls_path, enclosing_cls / TypeParams);

    // Build TypeParams for the new class.
    Node typeparams = TypeParams;

    for (auto& ftp : free_tps)
      typeparams << (TypeParam << (Ident ^ ftp.name));

    // Build TypeArgs for internal use (self type): FQ refs to
    // the new class's own type params.
    Node internal_ta = TypeArgs;

    for (auto& ftp : free_tps)
    {
      Node tp_tn = TypeName;

      for (auto& s : cls_path)
        tp_tn << (NameElement << clone(s / Ident) << TypeArgs);

      tp_tn << (NameElement << (Ident ^ id) << TypeArgs);
      tp_tn << (NameElement << (Ident ^ ftp.name) << TypeArgs);
      internal_ta << (Type << tp_tn);
    }

    // Build TypeArgs for creation site: FQ refs to the enclosing
    // scope's type params (the originals).
    Node outer_ta = TypeArgs;

    for (auto& ftp : free_tps)
    {
      Node tp_tn = TypeName;

      for (auto& s : ftp.path)
        tp_tn << (NameElement << clone(s / Ident) << TypeArgs);

      tp_tn << (NameElement << (Ident ^ ftp.name) << TypeArgs);
      outer_ta << (Type << tp_tn);
    }

    // Build FQ TypeName for use inside the class (self type).
    Node fq_tn = TypeName;

    for (auto& s : cls_path)
    {
      if (s == enclosing_cls)
        fq_tn << (NameElement << clone(enclosing_cls / Ident) << clone(cls_ta));
      else
        fq_tn << (NameElement << clone(s / Ident) << TypeArgs);
    }

    fq_tn << (NameElement << (Ident ^ id) << clone(internal_ta));
    auto self_type = Type << clone(fq_tn);

    // Build FQ TypeName for the creation site call.
    Node fq_tn_create = TypeName;

    for (auto& s : cls_path)
    {
      if (s == enclosing_cls)
        fq_tn_create
          << (NameElement << clone(enclosing_cls / Ident) << clone(cls_ta));
      else
        fq_tn_create << (NameElement << clone(s / Ident) << TypeArgs);
    }

    fq_tn_create << (NameElement << (Ident ^ id) << clone(outer_ta));

    // Build the class body: fields, create, apply.
    Node classbody = ClassBody;
    Node create_params = Params;
    Node create_args = Args;
    Node new_args = NewArgs;
    Node stack_new_args = NewArgs;

    // Prepend self param to apply_params.
    // Use $self to avoid conflicts with a captured outer "self".
    auto full_apply_params = Params
      << (ParamDef << (Ident ^ "$self") << clone(self_type) << Body);

    for (auto& child : *apply_params)
      full_apply_params << child;

    for (auto& field : fields)
    {
      classbody << (FieldDef << (Ident ^ field.name) << clone(field.type));
      create_params
        << (ParamDef << (Ident ^ field.name) << clone(field.type) << Body);
      create_args << field.create_arg;
      new_args
        << (NewArg << (Ident ^ field.name) << (Expr << (LocalId ^ field.name)));
      // For blocks, build NewArgs with the actual creation-site expressions
      // instead of LocalId references (since there's no create method).
      stack_new_args
        << (NewArg << (Ident ^ field.name) << clone(field.create_arg));
    }

    Node class_def;
    Node create_expr;

    if (is_block)
    {
      // Blocks don't have a create method. The object is stack-allocated
      // directly at the call site, so it never escapes.
      class_def =
        ClassDef << None << (Ident ^ id) << typeparams << Where
                 << (classbody
                     << (Function << Rhs << (Ident ^ "apply") << TypeParams
                                  << full_apply_params << apply_ret_type
                                  << Where << apply_body));

      create_expr = Stack << (Type << clone(fq_tn_create)) << stack_new_args;
    }
    else
    {
      class_def =
        ClassDef << None << (Ident ^ id) << typeparams << Where
                 << (classbody
                     << (Function << Rhs << (Ident ^ "create") << TypeParams
                                  << create_params << self_type << Where
                                  << (Body << (Expr << (New << new_args))))
                     << (Function << Rhs << (Ident ^ "apply") << TypeParams
                                  << full_apply_params << apply_ret_type
                                  << Where << apply_body));

      create_expr =
        Call << (FuncName << *clone(fq_tn_create)
                          << (NameElement << (Ident ^ "create") << TypeArgs))
             << create_args;
    }

    // Rebind locals in the apply function body so that nested lambdas
    // processed later in this topdown pass can find them via lookup().
    // The WF check normally rebuilds symtabs between pass iterations,
    // but topdown processing of nested lambdas happens within the same
    // iteration, before the WF check runs.
    auto apply_func = (class_def / ClassBody)->back();
    assert(apply_func == Function);
    auto body = apply_func / Body;
    body->traverse([&](auto node) {
      if (node->in({Var, Let}))
      {
        node->bind((node / Ident)->location());
      }
      // Don't descend into nested lambdas.
      return node != Lambda;
    });

    return {class_def, create_expr};
  }
}
