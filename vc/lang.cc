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

  Nodes class_ancestry(Node top, Node node)
  {
    Nodes result;
    auto cls = node->parent({FlatClass, ClassDef});

    if (!cls)
      return result;

    if (cls == ClassDef)
    {
      while (cls)
      {
        result.push_back(cls);
        cls = cls->parent(ClassDef);
      }

      return result;
    }

    auto path = cls / ClassPath;

    for (auto it = path->rbegin(); it != path->rend(); ++it)
    {
      auto defs = top->look((*it / DefId)->location());

      for (auto& def : defs)
      {
        if (def == FlatClass)
        {
          result.push_back(def);
          break;
        }
      }
    }

    return result;
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

  void append_class_path_id_segment(std::string& id, std::string_view name)
  {
    id += std::to_string(name.size());
    id += ':';
    id += name;
    id += ';';
  }

  std::string encode_class_path_id(const Nodes& path)
  {
    std::string id;

    for (auto& cls : path)
      append_class_path_id_segment(id, (cls / Ident)->location().view());

    return id;
  }

  namespace
  {
    // Flat classes live directly under Top, keyed in its symbol table by
    // DefId. Resolving a name means finding the longest class path that
    // prefixes it, which is a hot inner loop for every type comparison. Index
    // the classes by their innermost name, with their class path spelled out
    // as views, so a candidate check is a handful of string comparisons and
    // touches no nodes.
    struct FlatClassEntry
    {
      Node flat;
      std::vector<std::string_view> path;
    };

    struct FlatClassIndex
    {
      Node top;
      size_t size = 0;
      std::vector<FlatClassEntry> entries;
      std::map<std::string_view, std::vector<size_t>> by_name;

      void rebuild(const Node& new_top)
      {
        top = new_top;
        size = new_top->size();
        entries.clear();
        by_name.clear();

        for (auto& flat : *new_top)
        {
          if (flat != FlatClass)
            continue;

          FlatClassEntry entry{flat, {}};

          for (auto& segment : *(flat / ClassPath))
            entry.path.push_back((segment / Ident)->location().view());

          by_name[entry.path.back()].push_back(entries.size());
          entries.push_back(std::move(entry));
        }
      }
    };

    thread_local FlatClassIndex flat_class_index;

    // Top's children are only replaced wholesale between passes, so the
    // identity of the Top node and its child count are enough to detect that
    // the index needs rebuilding.
    const FlatClassIndex& get_flat_class_index(const Node& top)
    {
      if (
        (flat_class_index.top != top) || (flat_class_index.size != top->size()))
        flat_class_index.rebuild(top);

      return flat_class_index;
    }

    std::vector<std::string_view> name_idents(const Node& name)
    {
      std::vector<std::string_view> idents;
      idents.reserve(name->size());

      for (auto& elem : *name)
        idents.push_back((elem / Ident)->location().view());

      return idents;
    }

    Node find_flat_class_by_exact_prefix(
      const FlatClassIndex& index,
      const std::vector<std::string_view>& idents,
      size_t len)
    {
      auto find = index.by_name.find(idents[len - 1]);

      if (find == index.by_name.end())
        return {};

      for (auto& candidate : find->second)
      {
        auto& entry = index.entries[candidate];

        if (entry.path.size() != len)
          continue;

        if (std::equal(entry.path.begin(), entry.path.end(), idents.begin()))
          return entry.flat;
      }

      return {};
    }
  }

  Node find_flat_class_by_longest_prefix(
    Node top, const Node& name, size_t& consumed)
  {
    // Keep the longest class path that prefixes the name, so a class always
    // wins over a shorter enclosing scope of the same name.
    auto& index = get_flat_class_index(top);
    auto idents = name_idents(name);
    consumed = 0;

    for (size_t len = idents.size(); len > 0; len--)
    {
      auto flat = find_flat_class_by_exact_prefix(index, idents, len);

      if (flat)
      {
        consumed = len;
        return flat;
      }
    }

    return {};
  }

  ClassPrefixes find_flat_class_prefixes(Node top, const Node& name)
  {
    auto& index = get_flat_class_index(top);
    auto idents = name_idents(name);
    ClassPrefixes result;

    for (size_t len = idents.size(); len > 0; len--)
    {
      auto flat = find_flat_class_by_exact_prefix(index, idents, len);

      if (flat)
        result.emplace_back(flat, len);
    }

    return result;
  }

  Node find_def(Node top, const Node& name)
  {
    assert(name->in({FuncName, TypeName}));

    if ((name == TypeName) && (name->size() == 1))
    {
      auto def = find_typeparam_def(top, name);

      if (def)
        return def;
    }

    size_t consumed = 0;
    auto flat = find_flat_class_by_longest_prefix(top, name, consumed);

    if (flat)
    {
      auto it = name->begin();
      std::advance(it, consumed);
      return find_def_from(flat, name, it, name->end());
    }

    return find_def_from(top, name, name->begin(), name->end());
  }

  Node find_typeparam_def(Node top, const Node& name)
  {
    assert(name->in({FuncName, TypeName}));
    auto count = name->size();

    if (name == FuncName)
    {
      if (count < 2)
        return {};

      count--;
    }

    if (count == 1)
    {
      auto ident = name->front() / Ident;
      auto scope =
        name->parent({Top, FlatClass, ClassDef, TypeAlias, Function});

      while (scope)
      {
        for (auto& def : scope->look(ident->location()))
        {
          if (def->in({ClassDef, TypeAlias, TypeParam}))
            return def == TypeParam ? def : Node{};
        }

        scope = scope->parent({Top, FlatClass, ClassDef, TypeAlias, Function});
      }

      return {};
    }

    Node path = TypeName;
    size_t index = 0;

    for (auto& elem : *name)
    {
      if (index++ == count)
        break;

      path << clone(elem);
    }

    auto def = find_def(top, path);
    return def && (def == TypeParam) ? def : Node{};
  }

  void
  collect_func_defs_from_path(Node def, NodeIt it, NodeIt end, Nodes& result)
  {
    auto& elem = *it;
    assert(elem == NameElement);
    auto defs = def->look((elem / Ident)->location());
    bool is_last = (it + 1 == end);

    for (auto& candidate : defs)
    {
      if (is_last)
      {
        if (
          candidate == Function &&
          std::find(result.begin(), result.end(), candidate) == result.end())
          result.push_back(candidate);
      }
      else
      {
        collect_func_defs_from_path(candidate, it + 1, end, result);
      }
    }
  }

  Nodes find_func_defs(Node top, const Node& funcname)
  {
    assert(funcname == FuncName);
    Nodes result;

    if (!funcname->empty())
    {
      size_t consumed = 0;
      auto flat = find_flat_class_by_longest_prefix(top, funcname, consumed);

      if (flat)
      {
        auto it = funcname->begin();
        std::advance(it, consumed);

        if (it != funcname->end())
          collect_func_defs_from_path(flat, it, funcname->end(), result);
      }
      else
      {
        collect_func_defs_from_path(
          top, funcname->begin(), funcname->end(), result);
      }
    }

    return result;
  }

  Node find_func_def(Node top, const Node& funcname, size_t arity, Node hand)
  {
    assert(funcname == FuncName);
    auto defs = find_func_defs(top, funcname);

    for (auto& def : defs)
    {
      if (
        (!hand || (def / Lhs)->type() == hand->type()) &&
        (def / Params)->size() == arity)
        return def;
    }

    if (hand && hand->type() == Rhs)
    {
      for (auto& def : defs)
      {
        if ((def / Lhs)->type() == Once && (def / Params)->size() == arity)
          return def;
      }
    }

    return {};
  }

  // Build a fully-qualified, position-independent TypeName for a TypeParam
  // definition node in a flat class. A bare reference like "B" is only
  // resolvable via find_typeparam_def's scope walk relative to its own
  // lexical position in the tree. Type expressions are routinely copied out
  // of that position -- into another scope, into a freshly built parentless
  // expression, or into a clone of a function body -- at which point the bare
  // name can no longer be resolved. Qualifying it keeps it resolvable via
  // find_def from anywhere.
  Node fq_typeparam_ref(const Node& tp)
  {
    auto owner = tp->parent({FlatClass, TypeAlias, Function});

    if (!owner)
      return {};

    auto cls = (owner == FlatClass) ? owner : owner->parent(FlatClass);

    if (!cls)
      return {};

    Node tn = TypeName;

    for (auto& segment : *(cls / ClassPath))
      tn << (NameElement << clone(segment / Ident) << TypeArgs);

    if (owner != FlatClass)
      tn << (NameElement << clone(owner / Ident) << TypeArgs);

    tn << (NameElement << clone(tp / Ident) << TypeArgs);
    return tn;
  }

  // Recursively qualify any bare TypeParam reference within a type (see
  // fq_typeparam_ref) so it stays resolvable via find_def once the type is
  // detached from the tree position it was read from -- e.g. stored as a
  // substitution value and later cloned into an unrelated scope. Leaves
  // everything else (concrete classes, references that don't resolve to a
  // TypeParam, etc.) as a plain clone.
  // Core of qualify_typeparam_refs, operating on a bare (unwrapped) type
  // node and returning a bare node in kind. Kept separate from the public,
  // Type-wrapped qualify_typeparam_refs so that Union/Isect/TupleType
  // members (stored bare, not wrapped in Type) can be qualified without
  // first cloning them into a throwaway Type wrapper -- cloning first
  // would detach them from the tree, and find_typeparam_def needs that
  // attachment to identify which TypeParam a bare reference resolves to.
  Node qualify_typeparam_refs_bare(Node top, const Node& inner)
  {
    if (inner == TypeName)
    {
      if (inner->size() == 1)
      {
        auto tp_def = find_typeparam_def(top, inner);
        if (tp_def && tp_def == TypeParam)
        {
          auto fq = fq_typeparam_ref(tp_def);
          if (fq)
            return fq;
        }
      }

      Node new_tn = TypeName;
      for (auto& elem : *inner)
      {
        Node new_ta = TypeArgs;
        for (auto& ta_child : *(elem / TypeArgs))
          new_ta << qualify_typeparam_refs(top, ta_child);
        new_tn << (NameElement << clone(elem / Ident) << new_ta);
      }
      return new_tn;
    }

    if (inner->in({Union, Isect, TupleType}))
    {
      Node new_inner = inner->type();
      for (auto& child : *inner)
        new_inner << qualify_typeparam_refs_bare(top, child);
      return new_inner;
    }

    return clone(inner);
  }

  Node qualify_typeparam_refs(Node top, const Node& type_node)
  {
    if (type_node != Type)
      return clone(type_node);

    return Type << qualify_typeparam_refs_bare(top, type_node->front());
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

  Node unknown_typeargs(Node tps)
  {
    Node ta = TypeArgs;

    for (size_t i = 0; i < tps->size(); i++)
      ta << (Type << Unknown);

    return ta;
  }

  Node fq_scope_typeargs(Node scope)
  {
    return fq_typeargs(scope_path(scope), scope / TypeParams);
  }

  // Build a name for a lexical scope path. Generic scopes carry symbolic
  // references to their own TypeParams; definition paths use separate
  // helpers because they require empty TypeArgs instead.
  Node make_fq_name(const Token& name_type, const Nodes& path)
  {
    assert((name_type == TypeName) || (name_type == FuncName));
    Node name = name_type;

    for (auto& scope : path)
      name << (NameElement << clone(scope / Ident) << fq_scope_typeargs(scope));

    return name;
  }

  Node make_selftype(Node node, bool fq)
  {
    auto cls = node->parent({FlatClass, ClassDef});
    assert(cls);

    if (cls == FlatClass)
    {
      Node tn = TypeName;

      for (auto& segment : *(cls / ClassPath))
      {
        tn
          << (NameElement << clone(segment / Ident)
                          << clone(segment / TypeArgs));
      }

      return Type << tn;
    }

    auto path = scope_path(cls);

    if (fq)
    {
      Node tn = make_fq_name(TypeName, path);
      return Type << tn;
    }

    Node tn = TypeName;

    for (auto& s : path)
      tn << (NameElement << clone(s / Ident) << make_typeargs(s / TypeParams));

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
    Node new_tn = make_fq_name(TypeName, cls_path);

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

    // Build TypeParams for the new class.
    Node typeparams = TypeParams;

    for (auto& ftp : free_tps)
      typeparams << (TypeParam << (Ident ^ ftp.name));

    // Build TypeArgs for internal use (self type): FQ refs to
    // the new class's own type params.
    Node internal_ta = TypeArgs;

    for (auto& ftp : free_tps)
    {
      Node tp_tn = make_fq_name(TypeName, cls_path);

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
    Node fq_tn = make_fq_name(TypeName, cls_path);

    fq_tn << (NameElement << (Ident ^ id) << clone(internal_ta));
    auto self_type = Type << clone(fq_tn);

    // Build FQ TypeName for the creation site call.
    Node fq_tn_create = make_fq_name(TypeName, cls_path);

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

    return {class_def, create_expr};
  }
}
