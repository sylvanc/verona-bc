#include "../lang.h"
#include "../subtype.h"

namespace vc
{
  // ===== Dispatch tables =====

  const std::map<std::string_view, Token> primitive_from_name = {
    {"none", None}, {"bool", Bool},
    {"i8", I8}, {"i16", I16}, {"i32", I32}, {"i64", I64},
    {"u8", U8}, {"u16", U16}, {"u32", U32}, {"u64", U64},
    {"ilong", ILong}, {"ulong", ULong}, {"isize", ISize}, {"usize", USize},
    {"f32", F32}, {"f64", F64},
  };

  const std::map<std::string_view, Token> ffi_primitive_from_name = {
    {"ptr", Ptr}, {"callback", Callback},
  };

  const std::initializer_list<Token> integer_types = {
    I8, I16, I32, I64, U8, U16, U32, U64, ILong, ULong, ISize, USize};

  const std::initializer_list<Token> float_types = {F32, F64};

  // Binary ops: result type = LHS type.
  const std::initializer_list<Token> propagate_lhs_ops = {
    Add, Sub, Mul, Div, Mod, Pow, And, Or, Xor, Shl, Shr, Min, Max,
    LogBase, Atan2};

  // Unary ops: result type = operand type.
  const std::initializer_list<Token> propagate_rhs_ops = {
    Neg, Abs, Ceil, Floor, Exp, Log, Sqrt, Cbrt, Sin, Cos, Tan,
    Asin, Acos, Atan, Sinh, Cosh, Tanh, Asinh, Acosh, Atanh, Read};

  // Ops with fixed result types.
  const std::map<Token, Token> fixed_result_type = {
    {Eq, Bool}, {Ne, Bool}, {Lt, Bool}, {Le, Bool},
    {Gt, Bool}, {Ge, Bool}, {IsInf, Bool}, {IsNaN, Bool}, {Not, Bool},
    {Bits, U64}, {Len, USize},
    {Const_E, F64}, {Const_Pi, F64}, {Const_Inf, F64}, {Const_NaN, F64},
    {GetRaise, U64}, {SetRaise, U64},
    {FreeCallback, None}, {AddExternal, None}, {RemoveExternal, None},
    {RegisterExternalNotify, None}, {Freeze, None},
    {ArrayCopy, None}, {ArrayFill, None}, {ArrayCompare, I64},
  };

  const std::map<Token, Token> fixed_ffi_result_type = {
    {MakePtr, Ptr}, {CallbackPtr, Ptr}, {MakeCallback, Callback},
  };

  // ===== Type constructors =====

  Node primitive_type(const Token& tok)
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ tok.str()) << TypeArgs));
  }

  Node ffi_primitive_type(const Token& tok)
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "ffi") << TypeArgs)
                   << (NameElement << (Ident ^ tok.str()) << TypeArgs));
  }

  Node string_type()
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "string") << TypeArgs));
  }

  Node ref_type(const Node& inner)
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "ref")
                                   << (TypeArgs << clone(inner))));
  }

  Node cown_type(const Node& inner)
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "cown")
                                   << (TypeArgs << clone(inner))));
  }

  // ===== Type predicates =====

  bool is_default_type(const Node& type)
  {
    return type && !type->empty() &&
      type->front()->in({DefaultInt, DefaultFloat});
  }

  Node resolve_default(const Node& type)
  {
    if (!type || type->empty())
      return type;
    if (type->front() == DefaultInt)
      return primitive_type(U64);
    if (type->front() == DefaultFloat)
      return primitive_type(F64);
    return type;
  }

  Node extract_wrapper_inner(const Node& type_node, std::string_view wrapper)
  {
    if (type_node != Type)
      return {};
    auto inner = type_node->front();
    if (inner != TypeName || inner->size() != 2)
      return {};
    if ((inner->front() / Ident)->location().view() != "_builtin")
      return {};
    if ((inner->back() / Ident)->location().view() != wrapper)
      return {};
    auto ta = inner->back() / TypeArgs;
    if (ta->size() != 1)
      return {};
    return clone(ta->front());
  }

  Node extract_ref_inner(const Node& type_node)
  {
    return extract_wrapper_inner(type_node, "ref");
  }

  Node extract_cown_inner(const Node& type_node)
  {
    return extract_wrapper_inner(type_node, "cown");
  }

  Node extract_primitive(const Node& type_node)
  {
    if (type_node != Type)
      return {};
    auto inner = type_node->front();
    if (inner == DefaultInt)
      return U64;
    if (inner == DefaultFloat)
      return F64;
    if (inner != TypeName)
      return {};
    auto first = (inner->front() / Ident)->location().view();
    if (first != "_builtin")
      return {};
    if (inner->size() == 2)
    {
      auto it = primitive_from_name.find(
        (inner->back() / Ident)->location().view());
      return (it != primitive_from_name.end()) ? Node{it->second} : Node{};
    }
    if (inner->size() == 3)
    {
      if ((inner->at(1) / Ident)->location().view() != "ffi")
        return {};
      auto it = ffi_primitive_from_name.find(
        (inner->at(2) / Ident)->location().view());
      return (it != ffi_primitive_from_name.end()) ? Node{it->second} : Node{};
    }
    return {};
  }

  Node extract_callable_primitive(const Node& type_node)
  {
    auto prim = extract_primitive(type_node);
    if (prim)
      return prim;
    if (type_node != Type)
      return {};
    auto inner = type_node->front();
    if (!inner->in({Union, Isect}))
      return {};
    Node candidate;
    for (auto& component : *inner)
    {
      auto p = extract_primitive(Type << clone(component));
      if (!p)
        continue;
      if (candidate && candidate->type() != p->type())
        return {};
      candidate = p;
    }
    return candidate;
  }

  Node extract_wrapper_primitive(const Node& type_node)
  {
    auto inner = extract_ref_inner(type_node);
    if (!inner)
      inner = extract_cown_inner(type_node);
    return inner ? extract_callable_primitive(inner) : Node{};
  }

  Node default_literal_type(const Node& lit)
  {
    if (lit->in({True, False}))
      return Bool;
    if (lit == None)
      return None;
    if (lit->in({Bin, Oct, Int, Hex, Char}))
      return DefaultInt;
    if (lit->in({Float, HexFloat}))
      return DefaultFloat;
    assert(false && "unhandled literal type");
    return {};
  }

  static bool contains_typevar(const Node& type_node)
  {
    if (!type_node)
      return false;
    bool found = false;
    type_node->traverse([&](auto node) {
      if (node == TypeVar)
        found = true;
      return !found;
    });
    return found;
  }

  Node direct_typeparam(Node top, const Node& type_node)
  {
    if (type_node != Type)
      return {};
    auto inner = type_node->front();
    if (inner != TypeName)
      return {};
    auto def = find_def(top, inner);
    return (def && def == TypeParam) ? def : Node{};
  }

  // ===== Type environment =====

  struct LocalTypeInfo
  {
    Node type;
    bool is_fixed;
    Node call_node; // Call/CallDyn that produced this (for cross-function prop)
  };

  using TypeEnv = std::map<Location, LocalTypeInfo>;

  // ===== Type lattice: merge =====

  // Forward declaration for use in extract_constraints.
  Node apply_subst(Node top, const Node& type_node, const NodeMap<Node>& subst);

  struct MethodInfo
  {
    Node func;
    NodeMap<Node> subst;
  };

  MethodInfo resolve_method(
    Node top,
    const Node& receiver_type,
    const Node& method_ident,
    Token hand,
    size_t arity,
    const Node& method_typeargs);
  Node resolve_method_return_type(
    Node top,
    const Node& receiver_type,
    const Node& method_ident,
    Token hand,
    size_t arity,
    const Node& method_typeargs);
  MethodInfo resolve_callable_method(
    Node top,
    const Node& receiver_type,
    const Node& method_ident,
    Token hand,
    size_t arity,
    const Node& method_typeargs);

  // Returns the merged type, or {} if no change from existing.
  static Node merge_type(
    const Node& existing, const Node& incoming, Node top)
  {
    if (!existing || existing->empty() || existing->front() == TypeVar)
    {
      // Both TypeVar → no change.
      if (incoming && !incoming->empty() && incoming->front() == TypeVar)
        return {};
      return clone(incoming);
    }
    if (!incoming || incoming->empty() || incoming->front() == TypeVar)
      return {};

    if (existing == incoming)
      return {};

    // Default yields to compatible concrete primitive.
    // Checked BEFORE Subtype.invariant because AxiomFalse makes
    // Subtype consider DefaultInt invariant with i32.
    if (is_default_type(existing) && !is_default_type(incoming))
    {
      auto prim = extract_primitive(incoming);
      if (prim)
      {
        bool compat =
          (existing->front() == DefaultInt && prim->in(integer_types)) ||
          (existing->front() == DefaultFloat && prim->in(float_types));
        if (compat)
          return clone(incoming);
      }
    }

    if (is_default_type(incoming) && !is_default_type(existing))
    {
      auto prim = extract_primitive(existing);
      if (prim)
      {
        bool compat =
          (incoming->front() == DefaultInt && prim->in(integer_types)) ||
          (incoming->front() == DefaultFloat && prim->in(float_types));
        if (compat)
          return {};
      }
    }

    SequentCtx ctx{top, {}, {}};

    if (Subtype.invariant(ctx, existing, incoming))
      return {};

    if (Subtype(ctx, incoming, existing))
      return {};
    if (Subtype(ctx, existing, incoming))
      return clone(incoming);

    // Build union.
    auto e_inner = existing->front();
    auto i_inner = incoming->front();
    Node u = Union;

    if (e_inner == Union)
      for (auto& c : *e_inner)
        u << clone(c);
    else
      u << clone(e_inner);

    // If incoming is a concrete primitive, replace compatible Default
    // members in the union (DefaultInt→integer, DefaultFloat→float).
    auto inc_prim = extract_primitive(incoming);
    if (inc_prim && !is_default_type(incoming))
    {
      auto it = u->begin();
      while (it != u->end())
      {
        bool is_def_int = (*it) == DefaultInt;
        bool is_def_float = (*it) == DefaultFloat;
        if ((is_def_int && inc_prim->in(integer_types)) ||
            (is_def_float && inc_prim->in(float_types)))
          it = u->erase(it, std::next(it));
        else
          ++it;
      }
    }

    bool covered = false;
    for (auto& m : *u)
    {
      if (Subtype(ctx, incoming, Type << clone(m)))
      {
        covered = true;
        break;
      }
    }

    if (!covered)
    {
      auto it = u->begin();
      while (it != u->end())
      {
        if (Subtype(ctx, Type << clone(*it), incoming))
          it = u->erase(it, std::next(it));
        else
          ++it;
      }
      u << clone(i_inner);
    }

    return (u->size() == 1) ? Type << clone(u->front()) : Type << u;
  }

  // Merge a type into env at loc. Returns true if the type changed.
  static bool merge_env(
    TypeEnv& env,
    const Location& loc,
    const Node& type,
    Node top,
    Node call_node = {})
  {
    auto it = env.find(loc);
    if (it == env.end())
    {
      env[loc] = {clone(type), false, call_node};
      return true;
    }
    if (it->second.is_fixed)
      return false;

    auto merged = merge_type(it->second.type, type, top);
    if (!merged)
    {
      if (call_node && !it->second.call_node)
      {
        it->second.call_node = call_node;
        return true;
      }

      return false;
    }

    it->second.type = merged;
    if (call_node && !it->second.call_node)
      it->second.call_node = call_node;
    return true;
  }

  using SrcIndex = std::multimap<Location, Node>;

  static bool is_cascade_concrete(const Node& type)
  {
    return type && !type->empty() &&
      !type->front()->in({TypeVar, DefaultInt, DefaultFloat, Union});
  }

  static void enqueue_if_concrete(
    const TypeEnv& env,
    const Location& loc,
    std::deque<Location>& work,
    std::set<Location>& in_queue)
  {
    auto it = env.find(loc);
    if (it == env.end() || !is_cascade_concrete(it->second.type))
      return;
    if (in_queue.insert(loc).second)
      work.push_back(loc);
  }

  static SrcIndex build_src_index(const Node& body)
  {
    SrcIndex src_index;
    for (auto& stmt : *body)
    {
      if (stmt->in({Copy, Move}))
      {
        src_index.emplace((stmt / Rhs)->location(), stmt);
      }
      else if (stmt == Lookup)
      {
        src_index.emplace((stmt / Rhs)->location(), stmt);
      }
      else if (stmt->in({ArrayRef, ArrayRefConst, ArrayRefFromEnd, SplatOp}))
      {
        src_index.emplace(((stmt / Arg) / Rhs)->location(), stmt);
      }
      else if (stmt == Load)
      {
        src_index.emplace((stmt / Rhs)->location(), stmt);
      }
      else if (stmt->in({CallDyn, TryCallDyn}))
      {
        src_index.emplace((stmt / Rhs)->location(), stmt);
        for (auto& arg : *(stmt / Args))
          src_index.emplace((arg / Rhs)->location(), stmt);
      }
    }
    return src_index;
  }

  static void run_dependency_cascade(
    TypeEnv& env,
    const Node& body,
    Node top,
    std::map<Location, Node>& lookup_stmts)
  {
    auto src_index = build_src_index(body);
    std::map<Location, Node> const_defs;
    std::deque<Location> work;
    std::set<Location> in_queue;

    for (auto& stmt : *body)
      if (stmt == Const)
        const_defs[(stmt / LocalId)->location()] = stmt;

    auto refine_local_const =
      [&](const Location& loc, const Node& expected) -> bool {
      auto const_it = const_defs.find(loc);
      if (const_it == const_defs.end())
        return false;

      auto env_it = env.find(loc);
      if (env_it == env.end() || !is_default_type(env_it->second.type))
        return false;

      auto expected_prim = extract_primitive(expected);
      if (!expected_prim)
        return false;

      bool compatible =
        (env_it->second.type->front() == DefaultInt &&
         expected_prim->in(integer_types)) ||
        (env_it->second.type->front() == DefaultFloat &&
         expected_prim->in(float_types));
      if (!compatible)
        return false;

      env_it->second.type = primitive_type(expected_prim->type());
      auto const_stmt = const_it->second;
      if (const_stmt->size() == 3)
      {
        auto old_type = const_stmt->at(1);
        if (old_type->type() != expected_prim->type())
          const_stmt->replace(old_type, expected_prim->type());
      }
      else
      {
        auto dst = const_stmt->front();
        auto lit = const_stmt->back();
        const_stmt->erase(const_stmt->begin(), const_stmt->end());
        const_stmt << dst << expected_prim->type() << lit;
      }

      return true;
    };

    for (auto& [loc, info] : env)
      if (is_cascade_concrete(info.type))
        enqueue_if_concrete(env, loc, work, in_queue);

    while (!work.empty())
    {
      auto loc = work.front();
      work.pop_front();
      in_queue.erase(loc);

      auto [begin, end] = src_index.equal_range(loc);
      for (auto it = begin; it != end; ++it)
      {
        auto stmt = it->second;

        if (stmt->in({Copy, Move}))
        {
          auto src_loc = (stmt / Rhs)->location();
          auto src_it = env.find(src_loc);
          if (src_it == env.end())
            continue;

          auto dst_loc = (stmt / LocalId)->location();
          if (merge_env(
                env, dst_loc, src_it->second.type, top, src_it->second.call_node))
            enqueue_if_concrete(env, dst_loc, work, in_queue);
        }
        else if (stmt == Lookup)
        {
          auto src_loc = (stmt / Rhs)->location();
          auto src_it = env.find(src_loc);
          if (src_it == env.end() || is_default_type(src_it->second.type))
            continue;

          auto hand = (stmt / Lhs)->type();
          auto method_ident = stmt / Ident;
          auto method_ta = stmt / TypeArgs;
          auto arity = from_chars_sep_v<size_t>(stmt / Int);
          auto ret = resolve_method_return_type(
            top, src_it->second.type, method_ident, hand, arity, method_ta);
          if (!ret)
            continue;

          auto dst_loc = (stmt / LocalId)->location();
          if (merge_env(env, dst_loc, ret, top))
            enqueue_if_concrete(env, dst_loc, work, in_queue);
        }
        else if (stmt->in({ArrayRef, ArrayRefConst}))
        {
          auto dst_loc = (stmt / LocalId)->location();
          auto src_loc = ((stmt / Arg) / Rhs)->location();
          auto src_it = env.find(src_loc);
          if (src_it == env.end())
            continue;

          if (stmt == ArrayRefConst)
          {
            auto index = from_chars_sep_v<size_t>(stmt / Rhs);
            auto inner = src_it->second.type->front();
            if (inner == TupleType && index < inner->size())
            {
              if (merge_env(
                    env, dst_loc, ref_type(Type << clone(inner->at(index))), top))
                enqueue_if_concrete(env, dst_loc, work, in_queue);
              continue;
            }
          }

          if (merge_env(env, dst_loc, ref_type(clone(src_it->second.type)), top))
            enqueue_if_concrete(env, dst_loc, work, in_queue);
        }
        else if (stmt == ArrayRefFromEnd)
        {
          auto dst_loc = (stmt / LocalId)->location();
          auto src_loc = ((stmt / Arg) / Rhs)->location();
          auto src_it = env.find(src_loc);
          if (src_it == env.end())
            continue;

          auto offset = from_chars_sep_v<size_t>(stmt / Rhs);
          auto inner = src_it->second.type->front();
          if (inner == TupleType && offset > 0 && offset <= inner->size())
          {
            auto index = inner->size() - offset;
            if (merge_env(
                  env, dst_loc, ref_type(Type << clone(inner->at(index))), top))
              enqueue_if_concrete(env, dst_loc, work, in_queue);
          }
          else if (
            merge_env(env, dst_loc, ref_type(clone(src_it->second.type)), top))
          {
            enqueue_if_concrete(env, dst_loc, work, in_queue);
          }
        }
        else if (stmt == Load)
        {
          auto dst_loc = (stmt / LocalId)->location();
          auto src_it = env.find((stmt / Rhs)->location());
          if (src_it == env.end())
            continue;

          auto inner = extract_ref_inner(src_it->second.type);
          if (inner && merge_env(env, dst_loc, inner, top))
            enqueue_if_concrete(env, dst_loc, work, in_queue);
        }
        else if (stmt == SplatOp)
        {
          auto dst_loc = (stmt / LocalId)->location();
          auto src_loc = ((stmt / Arg) / Rhs)->location();
          auto src_it = env.find(src_loc);
          if (src_it == env.end())
            continue;

          auto inner = src_it->second.type->front();
          if (inner != TupleType)
            continue;

          auto before = from_chars_sep_v<size_t>(stmt / Lhs);
          auto after = from_chars_sep_v<size_t>(stmt / Rhs);
          if (before + after > inner->size())
            continue;

          auto remaining = inner->size() - before - after;
          Node splat_type;
          if (remaining == 0)
            splat_type = primitive_type(None);
          else if (remaining == 1)
            splat_type = Type << clone(inner->at(before));
          else
          {
            Node tup = TupleType;
            for (size_t i = before; i < before + remaining; i++)
              tup << clone(inner->at(i));
            splat_type = Type << tup;
          }

          if (merge_env(env, dst_loc, splat_type, top))
            enqueue_if_concrete(env, dst_loc, work, in_queue);
        }
        else if (stmt->in({CallDyn, TryCallDyn}))
        {
          auto dst_loc = (stmt / LocalId)->location();
          auto src_loc = (stmt / Rhs)->location();
          auto src_it = env.find(src_loc);
          if (src_it != env.end())
          {
            auto call_node = is_default_type(src_it->second.type) ? stmt : Node{};
            if (merge_env(env, dst_loc, src_it->second.type, top, call_node))
              enqueue_if_concrete(env, dst_loc, work, in_queue);
          }

          auto lookup_it = lookup_stmts.find(src_loc);
          if (lookup_it == lookup_stmts.end())
            continue;

          auto lookup_node = lookup_it->second;
          auto recv_loc = (lookup_node / Rhs)->location();
          auto recv_it = env.find(recv_loc);
          if (recv_it == env.end() || is_default_type(recv_it->second.type))
            continue;

          auto hand = (lookup_node / Lhs)->type();
          auto method_ident = lookup_node / Ident;
          auto method_ta = lookup_node / TypeArgs;
          auto arity = from_chars_sep_v<size_t>(lookup_node / Int);
          auto info = resolve_callable_method(
            top, recv_it->second.type, method_ident, hand, arity, method_ta);
          if (!info.func)
            continue;

          auto params = info.func / Params;
          auto args = stmt / Args;
          for (size_t i = 0; i < params->size() && i < args->size(); i++)
          {
            auto expected = apply_subst(top, params->at(i) / Type, info.subst);
            if (!expected || expected->front() == TypeVar)
              continue;

            auto arg_loc = (args->at(i) / Rhs)->location();
            snmalloc::UNUSED(refine_local_const(arg_loc, expected));
            if (merge_env(env, arg_loc, expected, top))
              enqueue_if_concrete(env, arg_loc, work, in_queue);
          }
        }
      }
    }
  }

  // ===== Generic type inference =====

  NodeMap<Node>
  build_class_subst(const Node& class_def, const Node& typename_node)
  {
    NodeMap<Node> subst;
    auto tps = class_def / TypeParams;
    auto ta = typename_node->back() / TypeArgs;
    if (tps->size() == ta->size())
      for (size_t i = 0; i < tps->size(); i++)
        subst[tps->at(i)] = ta->at(i);
    return subst;
  }

  void extract_constraints(
    Node top,
    const Node& f_inner,
    const Node& a_inner,
    NodeMap<LocalTypeInfo>& constraints,
    bool is_default)
  {
    // Formal is a TypeParam reference.
    if (f_inner == TypeName)
    {
      auto def = find_def(top, f_inner);
      if (def && def == TypeParam)
      {
        Node actual_type = Type << clone(a_inner);
        auto existing = constraints.find(def);
        if (existing == constraints.end())
          constraints[def] = {actual_type, false, {}};
        else if (is_default_type(existing->second.type) && !is_default)
          constraints[def] = {actual_type, false, {}};
        return;
      }
    }

    // Both TypeNames: structural or shape-aware matching.
    if (f_inner == TypeName && a_inner == TypeName)
    {
      bool structural = (f_inner->size() == a_inner->size());
      if (structural)
      {
        for (size_t i = 0; i < f_inner->size(); i++)
        {
          if (
            (f_inner->at(i) / Ident)->location().view() !=
            (a_inner->at(i) / Ident)->location().view())
          {
            structural = false;
            break;
          }
          if (
            (f_inner->at(i) / TypeArgs)->size() !=
            (a_inner->at(i) / TypeArgs)->size())
          {
            structural = false;
            break;
          }
        }
      }

      if (structural)
      {
        for (size_t i = 0; i < f_inner->size(); i++)
        {
          auto f_ta = f_inner->at(i) / TypeArgs;
          auto a_ta = a_inner->at(i) / TypeArgs;
          for (size_t j = 0; j < f_ta->size(); j++)
            extract_constraints(
              top, f_ta->at(j)->front(), a_ta->at(j)->front(),
              constraints, is_default);
        }
        return;
      }

      // Shape-aware matching.
      auto f_def = find_def(top, f_inner);
      auto a_def = find_def(top, a_inner);
      if (
        f_def && a_def && f_def == ClassDef && (f_def / Shape) == Shape &&
        a_def == ClassDef)
      {
        auto shape_to_formal = build_class_subst(f_def, f_inner);
        if (!shape_to_formal.empty())
        {
          auto actual_subst = build_class_subst(a_def, a_inner);
          for (auto& sf : *(f_def / ClassBody))
          {
            if (sf != Function)
              continue;
            auto mname = (sf / Ident)->location().view();
            auto hand = (sf / Lhs)->type();
            auto arity = (sf / Params)->size();
            for (auto& af : *(a_def / ClassBody))
            {
              if (af != Function)
                continue;
              if ((af / Ident)->location().view() != mname)
                continue;
              if ((af / Lhs)->type() != hand)
                continue;
              if ((af / Params)->size() != arity)
                continue;
              auto formal_ret = apply_subst(top, sf / Type, shape_to_formal);
              auto actual_ret = apply_subst(top, af / Type, actual_subst);
              extract_constraints(
                top, formal_ret->front(), actual_ret->front(),
                constraints, is_default);
              break;
            }
          }
        }
      }
      return;
    }

    // Union/Isect/TupleType: element-wise.
    if (
      f_inner->type() == a_inner->type() &&
      f_inner->size() == a_inner->size() &&
      f_inner->in({Union, Isect, TupleType}))
    {
      for (size_t i = 0; i < f_inner->size(); i++)
        extract_constraints(
          top, f_inner->at(i), a_inner->at(i), constraints, is_default);
    }
  }

  Node apply_subst(Node top, const Node& type_node, const NodeMap<Node>& subst)
  {
    if (type_node != Type || subst.empty())
      return clone(type_node);

    auto inner = type_node->front();

    if (inner == TypeName)
    {
      auto def = find_def(top, inner);
      if (def && def == TypeParam)
      {
        auto it = subst.find(def);
        if (it != subst.end())
          return clone(it->second);
      }

      Node new_tn = TypeName;
      for (auto& elem : *inner)
      {
        Node new_ta = TypeArgs;
        for (auto& ta_child : *(elem / TypeArgs))
          new_ta << apply_subst(top, ta_child, subst);
        new_tn << (NameElement << clone(elem / Ident) << new_ta);
      }
      return Type << new_tn;
    }

    if (inner->in({Union, Isect, TupleType}))
    {
      Node new_inner = inner->type();
      for (auto& child : *inner)
        new_inner << apply_subst(top, Type << clone(child), subst)->front();
      return Type << new_inner;
    }

    return clone(type_node);
  }

  // ===== Method resolution =====

  MethodInfo resolve_method(
    Node top,
    const Node& receiver_type,
    const Node& method_ident,
    Token hand,
    size_t arity,
    const Node& method_typeargs)
  {
    if (receiver_type != Type)
      return {};
    auto inner = receiver_type->front();
    if (inner != TypeName)
      return {};
    auto class_def = find_def(top, inner);
    Node subst_source = inner;
    if ((!class_def || class_def != ClassDef) && inner->size() > 1)
    {
      Node base = TypeName;
      for (size_t i = 0; i + 1 < inner->size(); i++)
        base << clone(inner->at(i));

      auto base_def = find_def(top, base);
      if (base_def && base_def == ClassDef)
      {
        class_def = base_def;
        subst_source = base;
      }
    }
    if (!class_def || class_def != ClassDef)
      return {};

    auto subst = build_class_subst(class_def, subst_source);
    auto method_name = method_ident->location().view();

    for (auto& child : *(class_def / ClassBody))
    {
      if (child != Function)
        continue;
      if ((child / Ident)->location().view() != method_name)
        continue;
      if ((child / Lhs)->type() != hand)
        continue;
      if ((child / Params)->size() != arity)
        continue;

      auto func_tps = child / TypeParams;
      if (
        !method_typeargs->empty() &&
        method_typeargs->size() == func_tps->size())
      {
        for (size_t i = 0; i < func_tps->size(); i++)
          subst[func_tps->at(i)] = method_typeargs->at(i);
      }
      return {child, std::move(subst)};
    }

    // Auto-deref ref[T] / cown[T].
    auto ref_inner = extract_ref_inner(receiver_type);
    if (!ref_inner)
      ref_inner = extract_cown_inner(receiver_type);
    if (ref_inner)
      return resolve_method(
        top, ref_inner, method_ident, hand, arity, method_typeargs);
    return {};
  }

  Node resolve_method_return_type(
    Node top,
    const Node& receiver_type,
    const Node& method_ident,
    Token hand,
    size_t arity,
    const Node& method_typeargs)
  {
    auto info = resolve_method(
      top, receiver_type, method_ident, hand, arity, method_typeargs);
    if (!info.func)
      return {};
    auto ret = apply_subst(top, info.func / Type, info.subst);

    // Rhs wrapper auto-gen: return type = lhs unwrapped ref.
    if (ret && ret->front() == TypeVar && hand == Rhs)
    {
      auto lhs_info = resolve_method(
        top, receiver_type, method_ident, Lhs, arity, method_typeargs);
      if (lhs_info.func)
      {
        auto lhs_ret =
          apply_subst(top, lhs_info.func / Type, lhs_info.subst);
        auto inner = extract_ref_inner(lhs_ret);
        if (inner)
          return inner;
      }
    }
    return ret;
  }

  MethodInfo resolve_callable_method(
    Node top,
    const Node& receiver_type,
    const Node& method_ident,
    Token hand,
    size_t arity,
    const Node& method_typeargs)
  {
    auto info = resolve_method(
      top, receiver_type, method_ident, hand, arity, method_typeargs);
    if (!info.func && hand == Rhs)
      info = resolve_method(
        top, receiver_type, method_ident, Lhs, arity, method_typeargs);
    return info;
  }

  void propagate_shape_to_lambda(
    Node top, const Node& shape_type, const Node& actual_type)
  {
    if (shape_type != Type || actual_type != Type)
      return;
    auto shape_inner = shape_type->front();
    auto actual_inner = actual_type->front();
    if (shape_inner != TypeName || actual_inner != TypeName)
      return;
    auto shape_def = find_def(top, shape_inner);
    auto actual_def = find_def(top, actual_inner);
    if (!shape_def || !actual_def)
      return;
    if (shape_def != ClassDef || actual_def != ClassDef)
      return;
    if ((shape_def / Shape) != Shape)
      return;

    auto shape_subst = build_class_subst(shape_def, shape_inner);
    for (auto& sf : *(shape_def / ClassBody))
    {
      if (sf != Function)
        continue;
      auto mname = (sf / Ident)->location().view();
      auto hand = (sf / Lhs)->type();

      for (auto& af : *(actual_def / ClassBody))
      {
        if (af != Function)
          continue;
        if ((af / Ident)->location().view() != mname)
          continue;
        if ((af / Lhs)->type() != hand)
          continue;
        if ((af / Params)->size() != (sf / Params)->size())
          continue;

        auto shape_params = sf / Params;
        auto actual_params = af / Params;
        for (size_t j = 0; j < shape_params->size(); j++)
        {
          auto ap = actual_params->at(j);
          auto apt = ap / Type;
          if (apt->front() != TypeVar)
            continue;
          auto spt = apply_subst(top, shape_params->at(j) / Type, shape_subst);
          if (!spt || spt->front() == TypeVar || spt->front() == TypeSelf)
            continue;
          ap->replace(apt, clone(spt));
        }

        auto actual_ret = af / Type;
        if (actual_ret->front() == TypeVar)
        {
          auto shape_ret = apply_subst(top, sf / Type, shape_subst);
          if (shape_ret && shape_ret->front() != TypeVar)
            af->replace(actual_ret, clone(shape_ret));
        }
        break;
      }
    }
  }

  // ===== Cross-function propagation =====

  struct ScopeInfo
  {
    Node name_elem;
    Node def;
  };

  Node navigate_call(Node call, Node top, std::vector<ScopeInfo>& scopes)
  {
    auto funcname = call / FuncName;
    auto args = call / Args;
    auto func_def = find_func_def(top, funcname, args->size(), call / Lhs);
    if (!func_def)
      return {};

    Node def = top;
    for (auto it = funcname->begin(); it != funcname->end(); ++it)
    {
      bool is_last = (it + 1 == funcname->end());
      if (is_last)
      {
        scopes.push_back({*it, func_def});
      }
      else
      {
        auto defs = def->look(((*it) / Ident)->location());
        def = defs.front();
        scopes.push_back({*it, def});
      }
    }
    return func_def;
  }

  void backward_refine_call(
    Node prior_call,
    const Node& expected_type,
    TypeEnv& env,
    Node top)
  {
    std::vector<ScopeInfo> scopes;
    auto func_def = navigate_call(prior_call, top, scopes);
    if (!func_def)
      return;

    auto args = prior_call / Args;
    auto params = func_def / Params;
    auto ret_type = func_def / Type;

    // Extract TypeParam constraints from return type vs expected type.
    NodeMap<LocalTypeInfo> constraints;
    extract_constraints(
      top, ret_type->front(), expected_type->front(), constraints, false);

    // Reverse shape matching: concrete return → shape expected.
    if (constraints.empty() && ret_type->front() == TypeName &&
        expected_type->front() == TypeName)
    {
      auto ret_def = find_def(top, ret_type->front());
      auto exp_def = find_def(top, expected_type->front());
      if (ret_def && exp_def && ret_def == ClassDef &&
          (ret_def / Shape) != Shape && exp_def == ClassDef &&
          (exp_def / Shape) == Shape)
      {
        auto shape_to_concrete =
          build_class_subst(exp_def, expected_type->front());
        if (!shape_to_concrete.empty())
        {
          for (auto& sf : *(exp_def / ClassBody))
          {
            if (sf != Function)
              continue;
            auto mname = (sf / Ident)->location().view();
            auto hand = (sf / Lhs)->type();
            auto arity = (sf / Params)->size();
            for (auto& cf : *(ret_def / ClassBody))
            {
              if (cf != Function)
                continue;
              if ((cf / Ident)->location().view() != mname)
                continue;
              if ((cf / Lhs)->type() != hand)
                continue;
              if ((cf / Params)->size() != arity)
                continue;
              auto concrete_ret =
                apply_subst(top, sf / Type, shape_to_concrete);
              extract_constraints(
                top, (cf / Type)->front(), concrete_ret->front(),
                constraints, false);
              break;
            }
          }
        }
      }
    }

    if (constraints.empty())
      return;

    // Update TypeArgs in the AST.
    for (auto& scope : scopes)
    {
      auto ta = scope.name_elem / TypeArgs;
      auto tps = scope.def / TypeParams;
      if (tps->empty())
        continue;

      bool all_constrained = true;
      Node new_ta = TypeArgs;
      for (auto& tp : *tps)
      {
        auto find = constraints.find(tp);
        if (find == constraints.end())
        {
          all_constrained = false;
          break;
        }
        new_ta << clone(find->second.type);
      }
      if (all_constrained)
        scope.name_elem->replace(ta, new_ta);
    }

    // Build substitution from updated TypeArgs.
    NodeMap<Node> subst;
    for (auto& scope : scopes)
    {
      auto ta = scope.name_elem / TypeArgs;
      auto tps = scope.def / TypeParams;
      if (!ta->empty() && ta->size() == tps->size())
        for (size_t i = 0; i < tps->size(); i++)
          subst[tps->at(i)] = ta->at(i);
    }

    // Backward: merge expected param types into arg env entries.
    for (size_t i = 0; i < params->size() && i < args->size(); i++)
    {
      auto formal = params->at(i) / Type;
      auto expected = apply_subst(top, formal, subst);
      if (expected)
      {
        auto arg_loc = (args->at(i) / Rhs)->location();
        merge_env(env, arg_loc, expected, top);
      }
    }

    // Update call result.
    auto result = apply_subst(top, ret_type, subst);
    if (result)
      merge_env(env, (prior_call / LocalId)->location(), result, top);
  }

  void backward_refine_calldyn(
    Node calldyn,
    const Node& expected_prim,
    TypeEnv& env,
    Node top,
    std::map<Location, Node>& lookup_stmts)
  {
    auto src = calldyn / Rhs;
    auto args = calldyn / Args;
    auto lookup_it = lookup_stmts.find(src->location());
    if (lookup_it == lookup_stmts.end())
      return;

    auto lookup_node = lookup_it->second;
    auto expected_type = primitive_type(expected_prim->type());

    // Merge expected type into each arg.
    for (auto& arg_node : *args)
      merge_env(env, (arg_node / Rhs)->location(), expected_type, top);

    // Merge into receiver.
    merge_env(env, (lookup_node / Rhs)->location(), expected_type, top);

    // Re-resolve the Lookup.
    auto lookup_src = lookup_node / Rhs;
    auto recv_it = env.find(lookup_src->location());
    if (recv_it != env.end())
    {
      auto hand = (lookup_node / Lhs)->type();
      auto method_ident = lookup_node / Ident;
      auto method_ta = lookup_node / TypeArgs;
      auto arity = from_chars_sep_v<size_t>(lookup_node / Int);
      auto ret = resolve_method_return_type(
        top, recv_it->second.type, method_ident, hand, arity, method_ta);
      if (ret)
      {
        merge_env(env, (lookup_node / LocalId)->location(), ret, top);
        merge_env(env, src->location(), ret, top);
        merge_env(env, (calldyn / LocalId)->location(), ret, top);
      }
    }
  }

  // ===== Typetest trace =====

  struct TypetestTrace
  {
    Node src;
    Node type;
    bool negated;
  };

  static std::optional<TypetestTrace>
  trace_typetest(const Node& cond_local, const Node& body)
  {
    auto target_loc = cond_local->location();
    bool negated = false;
    for (auto it = body->rbegin(); it != body->rend(); ++it)
    {
      auto& stmt = *it;
      if (stmt == Not)
      {
        auto dst = stmt / LocalId;
        if (dst->location() == target_loc)
        {
          target_loc = (stmt / Rhs)->location();
          negated = !negated;
        }
      }
      else if (stmt == Typetest)
      {
        auto dst = stmt / LocalId;
        if (dst->location() == target_loc)
          return TypetestTrace{stmt / Rhs, stmt / Type, negated};
      }
    }
    return std::nullopt;
  }

  // ===== Propagate call_node =====

  static void propagate_call_node(
    TypeEnv& env,
    const Location& loc,
    Node top,
    std::map<Location, Node>& lookup_stmts)
  {
    auto it = env.find(loc);
    if (it == env.end() || is_default_type(it->second.type) ||
        !it->second.call_node)
      return;

    auto call = it->second.call_node;
    if (call == Call)
      backward_refine_call(call, it->second.type, env, top);
    else if (call->in({CallDyn, TryCallDyn}))
    {
      auto prim = extract_primitive(it->second.type);
      if (prim)
        backward_refine_calldyn(call, prim, env, top, lookup_stmts);
    }
  }

  // ===== TypeArg inference for Call sites =====

  // Infers TypeArgs for a Call statement. Returns true if all constraints
  // came from default-typed args (enables call_node tracking).
  static bool infer_typeargs(
    Node call,
    Node func_def,
    std::vector<ScopeInfo>& scopes,
    TypeEnv& env,
    Node top)
  {
    auto args = call / Args;
    auto params = func_def / Params;

    bool needs_inference = false;
    for (auto& scope : scopes)
    {
      auto ta = scope.name_elem / TypeArgs;
      auto tps = scope.def / TypeParams;
      if (ta->empty() && !tps->empty())
      {
        needs_inference = true;
        break;
      }
    }

    if (!needs_inference)
      return false;

    // Collect constraints from arg types.
    NodeMap<LocalTypeInfo> constraints;
    for (size_t i = 0; i < params->size() && i < args->size(); i++)
    {
      auto arg_loc = (args->at(i) / Rhs)->location();
      auto arg_it = env.find(arg_loc);
      if (arg_it == env.end())
        continue;
      extract_constraints(
        top, (params->at(i) / Type)->front(),
        arg_it->second.type->front(), constraints,
        is_default_type(arg_it->second.type));
    }

    bool all_default = !constraints.empty();
    for (auto& [tp, info] : constraints)
      if (!is_default_type(info.type))
        all_default = false;

    // Fill TypeArgs.
    for (auto& scope : scopes)
    {
      auto ta = scope.name_elem / TypeArgs;
      auto tps = scope.def / TypeParams;
      if (tps->empty())
        continue;

      if (!ta->empty())
      {
        bool needs_reinfer = false;
        for (auto& t : *ta)
          if (direct_typeparam(top, t))
          {
            needs_reinfer = true;
            break;
          }
        if (!needs_reinfer)
          continue;
      }

      bool all_constrained = true;
      Node new_ta = TypeArgs;
      for (auto& tp : *tps)
      {
        auto find = constraints.find(tp);
        if (find == constraints.end())
        {
          all_constrained = false;
          break;
        }
        new_ta << clone(find->second.type);
      }
      if (all_constrained)
        scope.name_elem->replace(ta, new_ta);
    }

    return all_default;
  }

  // Push concrete arg types into TypeVar formal params (AST mutation).
  static void push_arg_types_to_params(
    Node func_def, const Node& args, TypeEnv& env, Node /*top*/)
  {
    auto params = func_def / Params;
    auto parent_cls = func_def->parent(ClassDef);

    for (size_t i = 0; i < params->size() && i < args->size(); i++)
    {
      auto param = params->at(i);
      auto formal_type = param / Type;
      if (!contains_typevar(formal_type))
        continue;

      Node resolved;

      // FieldDef takes priority.
      if (parent_cls)
      {
        auto pname = (param / Ident)->location().view();
        for (auto& child : *(parent_cls / ClassBody))
        {
          if (child != FieldDef)
            continue;
          if ((child / Ident)->location().view() != pname)
            continue;
          if (!contains_typevar(child / Type))
            resolved = child / Type;
          break;
        }
      }

      if (!resolved)
      {
        auto arg_loc = (args->at(i) / Rhs)->location();
        auto arg_it = env.find(arg_loc);
        if (arg_it == env.end() || contains_typevar(arg_it->second.type) ||
            is_default_type(arg_it->second.type))
          continue;
        resolved = arg_it->second.type;
      }

      param->replace(formal_type, clone(resolved));

      // Keep FieldDef in sync.
      if (parent_cls)
      {
        auto pname = (param / Ident)->location().view();
        for (auto& child : *(parent_cls / ClassBody))
        {
          if (child != FieldDef)
            continue;
          if ((child / Ident)->location().view() != pname)
            continue;
          if (contains_typevar(child / Type))
            child->replace(child / Type, clone(resolved));
          break;
        }
      }
    }
  }

  // ===== Transfer functions: process_label_body =====

  // Tuple tracking for NewArrayConst → ArrayRefConst → Store patterns.
  struct TupleTracking
  {
    size_t size;
    bool is_array_lit;
    std::vector<Node> element_types;
    std::vector<Location> element_value_locs;
  };

  struct LabelChanges
  {
    bool forward = false;
    bool backward = false;
  };

  static LabelChanges process_label_body(
    const Node& body,
    TypeEnv& env,
    TypeEnv& bwd,
    Node top,
    std::map<Location, Node>& lookup_stmts,
    std::vector<std::pair<Location, Location>>& typevar_aliases,
    std::map<Location, std::pair<Location, size_t>>& ref_to_tuple,
    std::map<Location, TupleTracking>& tuple_locals)
  {
    LabelChanges changes;
    std::map<Location, Node> const_defs;

    for (auto& stmt : *body)
      if (stmt == Const)
        const_defs[(stmt / LocalId)->location()] = stmt;

    auto merge = [&](const Location& loc, const Node& type,
                     Node call_node = {}) -> bool {
      bool c = merge_env(env, loc, type, top, call_node);
      if (c)
        changes.forward = true;
      return c;
    };

    auto refine_local_const =
      [&](const Location& loc, const Node& expected) -> bool {
      auto const_it = const_defs.find(loc);
      if (const_it == const_defs.end())
        return false;

      auto env_it = env.find(loc);
      if (env_it == env.end() || !is_default_type(env_it->second.type))
        return false;

      auto expected_prim = extract_primitive(expected);
      if (!expected_prim)
        return false;

      bool compatible =
        (env_it->second.type->front() == DefaultInt &&
         expected_prim->in(integer_types)) ||
        (env_it->second.type->front() == DefaultFloat &&
         expected_prim->in(float_types));
      if (!compatible)
        return false;

      auto refined = primitive_type(expected_prim->type());
      env_it->second.type = refined;
      changes.forward = true;

      auto const_stmt = const_it->second;
      if (const_stmt->size() == 3)
      {
        auto old_type = const_stmt->at(1);
        if (old_type->type() != expected_prim->type())
          const_stmt->replace(old_type, expected_prim->type());
      }
      else
      {
        auto dst = const_stmt->front();
        auto lit = const_stmt->back();
        const_stmt->erase(const_stmt->begin(), const_stmt->end());
        const_stmt << dst << expected_prim->type() << lit;
      }

      return true;
    };

    auto merge_bwd = [&](const Location& loc, const Node& type) -> bool {
      bool changed = false;

      if (merge_env(env, loc, type, top))
      {
        changes.forward = true;
        changed = true;
      }

      // Record backward constraint (only concrete, non-union types).
      if (
        type && !type->empty() &&
        !type->front()->in({TypeVar, DefaultInt, DefaultFloat, Union}))
      {
        auto bit = bwd.find(loc);
        if (bit == bwd.end())
        {
          bwd[loc] = {clone(type), false, {}};
          changes.backward = true;
          changed = true;
        }
        else
        {
          auto m = merge_type(bit->second.type, type, top);
          if (m)
          {
            bit->second.type = m;
            changes.backward = true;
            changed = true;
          }
        }
      }

      return changed;
    };

    for (auto& stmt : *body)
    {
      // ----- Const -----
      if (stmt == Const)
      {
        auto dst = stmt->front();
        Node type_tok;
        if (stmt->size() == 3)
          type_tok = stmt->at(1);
        else
          type_tok = default_literal_type(stmt->back());

        auto type = type_tok->in({DefaultInt, DefaultFloat})
          ? (Type << type_tok->type()) : primitive_type(type_tok->type());

        if (is_default_type(type))
        {
          auto it = env.find(dst->location());
          if (it != env.end() && !is_default_type(it->second.type))
          {
            auto prim = extract_primitive(it->second.type);
            if (
              prim &&
              ((type->front() == DefaultInt && prim->in(integer_types)) ||
               (type->front() == DefaultFloat && prim->in(float_types))))
            {
              type = clone(it->second.type);
            }
          }
        }

        merge(dst->location(), type);
      }
      // ----- ConstStr -----
      else if (stmt == ConstStr)
      {
        merge((stmt / LocalId)->location(), string_type());
      }
      // ----- Convert -----
      else if (stmt == Convert)
      {
        merge((stmt / LocalId)->location(), clone(stmt / Type));
      }
      // ----- Copy / Move -----
      else if (stmt->in({Copy, Move}))
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto src_loc = (stmt / Rhs)->location();
        auto src_it = env.find(src_loc);
        auto dst_it = env.find(dst_loc);

        // Track TypeVar aliases for post-convergence back-prop.
        if (src_it != env.end() && dst_it != env.end())
        {
          bool dst_tv = dst_it->second.type->front() == TypeVar;
          bool src_tv = src_it->second.type->front() == TypeVar;
          if (dst_tv != src_tv)
            typevar_aliases.push_back({dst_loc, src_loc});
        }

        // Forward: dst = merge(dst, src).
        if (src_it != env.end())
          merge(
            dst_loc, src_it->second.type, src_it->second.call_node);

        // Backward: src = merge(src, dst).
        dst_it = env.find(dst_loc);
        if (dst_it != env.end())
        {
          if (dst_it->second.is_fixed)
            snmalloc::UNUSED(refine_local_const(src_loc, dst_it->second.type));
          if (merge_bwd(src_loc, dst_it->second.type))
            propagate_call_node(env, src_loc, top, lookup_stmts);
        }
      }
      // ----- RegisterRef -----
      else if (stmt == RegisterRef)
      {
        auto src_it = env.find((stmt / Rhs)->location());
        if (src_it != env.end())
          merge(
            (stmt / LocalId)->location(),
            ref_type(clone(src_it->second.type)));
      }
      // ----- FieldRef -----
      else if (stmt == FieldRef)
      {
        auto arg_src = (stmt / Arg) / Rhs;
        auto obj_it = env.find(arg_src->location());
        if (obj_it != env.end())
        {
          auto inner = obj_it->second.type->front();
          if (inner == TypeName)
          {
            auto class_def = find_def(top, inner);
            if (class_def && class_def == ClassDef)
            {
              auto subst = build_class_subst(class_def, inner);
              auto fname = (stmt / FieldId)->location().view();
              for (auto& f : *(class_def / ClassBody))
              {
                if (f != FieldDef)
                  continue;
                if ((f / Ident)->location().view() != fname)
                  continue;
                auto ft = apply_subst(top, f / Type, subst);
                if (ft)
                  merge(
                    (stmt / LocalId)->location(), ref_type(ft));
                break;
              }
            }
          }
        }
      }
      // ----- Load -----
      else if (stmt == Load)
      {
        auto src_it = env.find((stmt / Rhs)->location());
        if (src_it != env.end())
        {
          auto inner = extract_ref_inner(src_it->second.type);
          if (inner)
            merge((stmt / LocalId)->location(), inner);
        }
      }
      // ----- Store -----
      else if (stmt == Store)
      {
        // WF: wfDst * wfSrc * Arg → LocalId(dst), Rhs(ref), Arg(val)
        auto dst_loc = (stmt / LocalId)->location();
        auto ref_loc = (stmt / Rhs)->location();
        auto val_loc = ((stmt / Arg) / Rhs)->location();

        auto ref_it = env.find(ref_loc);
        if (ref_it != env.end())
        {
          auto inner = extract_ref_inner(ref_it->second.type);
          if (inner)
          {
            // Forward: dst = inner type.
            merge(dst_loc, clone(inner));
            // Backward: refine stored value.
            if (merge_bwd(val_loc, clone(inner)))
              propagate_call_node(env, val_loc, top, lookup_stmts);

            // Track tuple element types.
            auto rtt = ref_to_tuple.find(ref_loc);
            if (rtt != ref_to_tuple.end())
            {
              auto& [tup_loc, idx] = rtt->second;
              auto tt = tuple_locals.find(tup_loc);
              if (tt != tuple_locals.end() && idx < tt->second.size)
              {
                auto val_it = env.find(val_loc);
                if (val_it != env.end())
                {
                  tt->second.element_types[idx] =
                    clone(val_it->second.type);
                  if (tt->second.is_array_lit)
                    tt->second.element_value_locs[idx] = val_loc;
                }
              }
            }
          }
        }
      }
      // ----- ArrayRef / ArrayRefConst -----
      else if (stmt->in({ArrayRef, ArrayRefConst}))
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto arg_loc = ((stmt / Arg) / Rhs)->location();
        auto src_it = env.find(arg_loc);

        if (stmt == ArrayRefConst)
        {
          auto index = from_chars_sep_v<size_t>(stmt / Rhs);
          // Track ref_to_tuple for Store-based element tracking.
          auto rtt = ref_to_tuple.find(arg_loc);
          if (rtt != ref_to_tuple.end())
            ref_to_tuple[dst_loc] = {rtt->second.first, index};
          else
            ref_to_tuple[dst_loc] = {arg_loc, index};

          // Resolve element if source is TupleType.
          if (src_it != env.end())
          {
            auto inner = src_it->second.type->front();
            if (inner == TupleType && index < inner->size())
            {
              merge(
                dst_loc,
                ref_type(Type << clone(inner->at(index))));
              continue;
            }
          }
        }

        if (src_it != env.end())
          merge(
            dst_loc,
            ref_type(clone(src_it->second.type)));
      }
      // ----- ArrayRefFromEnd -----
      else if (stmt == ArrayRefFromEnd)
      {
        auto arg_loc = ((stmt / Arg) / Rhs)->location();
        auto src_it = env.find(arg_loc);
        if (src_it != env.end())
        {
          auto inner = src_it->second.type->front();
          if (inner == TupleType)
          {
            auto offset = from_chars_sep_v<size_t>(stmt / Rhs);
            if (offset > 0 && offset <= inner->size())
            {
              auto index = inner->size() - offset;
              merge(
                (stmt / LocalId)->location(),
                ref_type(Type << clone(inner->at(index))));
              continue;
            }
          }

          merge(
            (stmt / LocalId)->location(),
            ref_type(clone(src_it->second.type)));
        }
      }
      // ----- SplatOp -----
      else if (stmt == SplatOp)
      {
        auto arg_loc = ((stmt / Arg) / Rhs)->location();
        auto src_it = env.find(arg_loc);
        if (src_it != env.end() && src_it->second.type->front() == TupleType)
        {
          auto inner = src_it->second.type->front();
          auto before = from_chars_sep_v<size_t>(stmt / Lhs);
          auto after = from_chars_sep_v<size_t>(stmt / Rhs);
          if (before + after <= inner->size())
          {
            auto remaining = inner->size() - before - after;
            if (remaining == 0)
            {
              merge((stmt / LocalId)->location(), primitive_type(None));
            }
            else if (remaining == 1)
            {
              merge(
                (stmt / LocalId)->location(), Type << clone(inner->at(before)));
            }
            else
            {
              Node tup = TupleType;
              for (size_t i = before; i < before + remaining; i++)
                tup << clone(inner->at(i));
              merge((stmt / LocalId)->location(), Type << tup);
            }
            continue;
          }
        }

        merge((stmt / LocalId)->location(), make_type());
      }
      // ----- NewArray / NewArrayConst -----
      else if (stmt->in({NewArray, NewArrayConst}))
      {
        auto dst_loc = (stmt / LocalId)->location();
        merge(dst_loc, clone(stmt / Type));

        if (stmt == NewArrayConst)
        {
          auto sz = from_chars_sep_v<size_t>(stmt / Rhs);
          bool is_lit =
            dst_loc.view().find("array") != std::string_view::npos;
          tuple_locals[dst_loc] =
            {sz, is_lit, std::vector<Node>(sz), std::vector<Location>(sz)};
          ref_to_tuple[dst_loc] = {dst_loc, 0};
        }
      }
      // ----- TypeAssertion -----
      else if (stmt == TypeAssertion)
      {
        auto loc = (stmt / LocalId)->location();
        merge(loc, clone(stmt / Type));
        auto it = env.find(loc);
        if (it != env.end())
          it->second.is_fixed = true;
      }
      // ----- New / Stack -----
      else if (stmt->in({New, Stack}))
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto new_type = stmt / Type;
        merge(dst_loc, clone(new_type));

        // Backward: constrain args from field types.
        auto inner = new_type->front();
        if (inner == TypeName)
        {
          auto class_def = find_def(top, inner);
          if (class_def && class_def == ClassDef)
          {
            auto subst = build_class_subst(class_def, inner);
            for (auto& na : *(stmt / NewArgs))
            {
              auto arg_loc = (na / Rhs)->location();
              auto fname = (na / Ident)->location().view();
              for (auto& f : *(class_def / ClassBody))
              {
                if (f != FieldDef)
                  continue;
                if ((f / Ident)->location().view() != fname)
                  continue;
                auto ft = apply_subst(top, f / Type, subst);
                if (ft && !contains_typevar(ft))
                {
                  snmalloc::UNUSED(refine_local_const(arg_loc, ft));
                  if (merge_bwd(arg_loc, ft))
                    propagate_call_node(env, arg_loc, top, lookup_stmts);
                }
                // Reverse: push concrete arg into TypeVar FieldDef.
                auto arg_it = env.find(arg_loc);
                if (arg_it != env.end() && contains_typevar(f / Type) &&
                    !contains_typevar(arg_it->second.type) &&
                    !is_default_type(arg_it->second.type))
                  f->replace(f / Type, clone(arg_it->second.type));
                break;
              }
            }
          }
        }
      }
      // ----- Binary ops (result = LHS type) -----
      else if (stmt->in(propagate_lhs_ops))
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto lhs_loc = (stmt / Lhs)->location();
        auto rhs_loc = (stmt / Rhs)->location();

        auto lhs_it = env.find(lhs_loc);
        if (lhs_it != env.end())
        {
          // Forward: result = lhs type.
          merge(dst_loc, clone(lhs_it->second.type));
          // Backward: refine rhs from lhs.
          if (merge_bwd(rhs_loc, clone(lhs_it->second.type)))
            propagate_call_node(env, rhs_loc, top, lookup_stmts);
        }

        // Backward from dst: refine lhs and rhs from dst (from prior
        // iteration's backward flow, e.g., Call backward refined dst).
        auto dst_it = env.find(dst_loc);
        if (dst_it != env.end() && !is_default_type(dst_it->second.type))
        {
          if (merge_bwd(lhs_loc, clone(dst_it->second.type)))
            propagate_call_node(env, lhs_loc, top, lookup_stmts);
          if (merge_bwd(rhs_loc, clone(dst_it->second.type)))
            propagate_call_node(env, rhs_loc, top, lookup_stmts);
        }
      }
      // ----- Unary ops (result = operand type) -----
      else if (stmt->in(propagate_rhs_ops))
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto src_loc = (stmt / Rhs)->location();

        auto src_it = env.find(src_loc);
        if (src_it != env.end())
          merge(dst_loc, clone(src_it->second.type));

        // Backward from dst.
        auto dst_it = env.find(dst_loc);
        if (dst_it != env.end() && !is_default_type(dst_it->second.type))
        {
          if (merge_bwd(src_loc, clone(dst_it->second.type)))
            propagate_call_node(env, src_loc, top, lookup_stmts);
        }
      }
      // ----- Fixed result types -----
      else if (auto frt = fixed_result_type.find(stmt->type());
               frt != fixed_result_type.end())
      {
        merge(
          (stmt / LocalId)->location(),
          primitive_type(frt->second));
      }
      else if (auto ffrt = fixed_ffi_result_type.find(stmt->type());
               ffrt != fixed_ffi_result_type.end())
      {
        merge(
          (stmt / LocalId)->location(),
          ffi_primitive_type(ffrt->second));
      }
      // ----- Call -----
      else if (stmt == Call)
      {
        std::vector<ScopeInfo> scopes;
        auto func_def = navigate_call(stmt, top, scopes);
        if (!func_def)
          continue;

        auto args = stmt / Args;
        auto params = func_def / Params;

        // TypeArg inference.
        bool all_default = infer_typeargs(stmt, func_def, scopes, env, top);

        // Build substitution.
        NodeMap<Node> subst;
        for (auto& scope : scopes)
        {
          auto ta = scope.name_elem / TypeArgs;
          auto tps = scope.def / TypeParams;
          if (!ta->empty() && ta->size() == tps->size())
            for (size_t i = 0; i < tps->size(); i++)
              subst[tps->at(i)] = ta->at(i);
        }

        // Forward: return type.
        auto ret = apply_subst(top, func_def / Type, subst);
        if (ret)
          merge(
            (stmt / LocalId)->location(), ret,
            all_default ? stmt : Node{});

        // Backward: param types into args.
        for (size_t i = 0; i < params->size() && i < args->size(); i++)
        {
          auto expected = apply_subst(top, params->at(i) / Type, subst);
          if (expected && expected->front() != TypeVar)
          {
            auto arg_loc = (args->at(i) / Rhs)->location();
            snmalloc::UNUSED(refine_local_const(arg_loc, expected));
            if (merge_bwd(arg_loc, expected))
              propagate_call_node(env, arg_loc, top, lookup_stmts);
          }
        }

        // Forward into callee: push arg types into TypeVar params.
        push_arg_types_to_params(func_def, args, env, top);

        // Shape-to-lambda propagation.
        for (size_t i = 0; i < params->size() && i < args->size(); i++)
        {
          auto pt = apply_subst(top, params->at(i) / Type, subst);
          if (pt && pt->front() != TypeVar)
          {
            auto arg_it = env.find((args->at(i) / Rhs)->location());
            if (arg_it != env.end())
              propagate_shape_to_lambda(top, pt, arg_it->second.type);
          }
        }
      }
      // ----- Lookup -----
      else if (stmt == Lookup)
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto src_it = env.find((stmt / Rhs)->location());
        if (src_it != env.end())
        {
          // When receiver is default-typed, propagate the default type
          // as the Lookup result. This makes the CallDyn result default,
          // enabling call_node tracking for backward refinement.
          if (is_default_type(src_it->second.type))
          {
            merge(dst_loc, clone(src_it->second.type));
          }
          else
          {
            auto hand = (stmt / Lhs)->type();
            auto method_ident = stmt / Ident;
            auto method_ta = stmt / TypeArgs;
            auto arity = from_chars_sep_v<size_t>(stmt / Int);
            auto ret = resolve_method_return_type(
              top, src_it->second.type, method_ident, hand, arity,
              method_ta);
            if (ret)
              merge(dst_loc, ret);
          }
        }
        lookup_stmts[dst_loc] = stmt;
      }
      // ----- CallDyn / TryCallDyn -----
      else if (stmt->in({CallDyn, TryCallDyn}))
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto src_loc = (stmt / Rhs)->location();
        auto args = stmt / Args;

        // Forward: result from Lookup.
        auto src_it = env.find(src_loc);
        if (src_it != env.end())
          merge(dst_loc, clone(src_it->second.type));

        // Set call_node if result is default-typed (per plan).
        auto dst_it = env.find(dst_loc);
        if (dst_it != env.end() && is_default_type(dst_it->second.type))
          dst_it->second.call_node = stmt;

        // Resolve method for backward refinement.
        // Skip when receiver is default-typed — the method resolution
        // would use the fallback type (u64/f64), pushing wrong types
        // into args. The correct type will be determined by backward
        // refinement from downstream constraints.
        auto lookup_it = lookup_stmts.find(src_loc);
        if (lookup_it != lookup_stmts.end())
        {
          auto lookup_node = lookup_it->second;
          auto recv_it = env.find((lookup_node / Rhs)->location());
          if (recv_it != env.end() && !is_default_type(recv_it->second.type))
          {
            auto hand = (lookup_node / Lhs)->type();
            auto method_ident = lookup_node / Ident;
            auto method_ta = lookup_node / TypeArgs;
            auto arity = from_chars_sep_v<size_t>(lookup_node / Int);
            auto info = resolve_callable_method(
              top, recv_it->second.type, method_ident, hand, arity,
              method_ta);

            if (info.func)
            {
              auto params = info.func / Params;

              // Backward: param types into args.
              for (size_t i = 0; i < params->size() && i < args->size(); i++)
              {
                auto expected =
                  apply_subst(top, params->at(i) / Type, info.subst);
                if (expected && expected->front() != TypeVar)
                {
                  auto arg_loc = (args->at(i) / Rhs)->location();
                  snmalloc::UNUSED(refine_local_const(arg_loc, expected));
                  if (merge_bwd(arg_loc, expected))
                    propagate_call_node(env, arg_loc, top, lookup_stmts);
                }
              }

              // Forward into callee: TypeVar params.
              push_arg_types_to_params(info.func, args, env, top);

              // Shape-to-lambda propagation.
              for (size_t i = 0; i < params->size() && i < args->size(); i++)
              {
                auto pt =
                  apply_subst(top, params->at(i) / Type, info.subst);
                if (pt && pt->front() != TypeVar)
                {
                  auto arg_it = env.find((args->at(i) / Rhs)->location());
                  if (arg_it != env.end())
                    propagate_shape_to_lambda(top, pt, arg_it->second.type);
                }
              }
            }
          }
        }
      }
      // ----- FFI -----
      else if (stmt == FFI)
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto sym_name = (stmt / SymbolId)->location();
        auto cls = body->parent(Function)->parent(ClassDef);

        while (cls)
        {
          bool found = false;
          for (auto& child : *(cls / ClassBody))
          {
            if (child != Lib)
              continue;
            for (auto& sym : *(child / Symbols))
            {
              if (sym != Symbol)
                continue;
              if ((sym / SymbolId)->location() != sym_name)
                continue;

              // Forward: return type.
              auto ret_type = sym / Type;
              if (!ret_type->empty())
                merge(dst_loc, clone(ret_type));

              // Backward: param types into args.
              auto ffi_params = sym / FFIParams;
              auto ffi_args = stmt / Args;
              auto fp = ffi_params->begin();
              auto fa = ffi_args->begin();
              while (fp != ffi_params->end() && fa != ffi_args->end())
              {
                snmalloc::UNUSED(refine_local_const((*fa)->location(), clone(*fp)));
                if (merge_bwd((*fa)->location(), clone(*fp)))
                  propagate_call_node(
                    env, (*fa)->location(), top, lookup_stmts);
                ++fp;
                ++fa;
              }
              found = true;
              break;
            }
            if (found)
              break;
          }
          if (found)
            break;
          cls = cls->parent(ClassDef);
        }
      }
      // ----- When -----
      else if (stmt == When)
      {
        auto dst_loc = (stmt / LocalId)->location();
        auto src_it = env.find((stmt / Rhs)->location());

        if (src_it != env.end())
        {
          auto apply_ret = src_it->second.type;
          stmt->replace(stmt / Type, clone(apply_ret));
          merge(dst_loc, cown_type(apply_ret));
        }

        // Set lambda params from cown types.
        auto lookup_it = lookup_stmts.find((stmt / Rhs)->location());
        if (lookup_it != lookup_stmts.end())
        {
          auto recv_it =
            env.find((lookup_it->second / Rhs)->location());
          if (recv_it != env.end())
          {
            auto recv_inner = recv_it->second.type->front();
            if (recv_inner == TypeName)
            {
              auto class_def = find_def(top, recv_inner);
              if (class_def && class_def == ClassDef)
              {
                Node apply_func;
                for (auto& child : *(class_def / ClassBody))
                {
                  if (child == Function &&
                      (child / Ident)->location().view() == "apply")
                  {
                    apply_func = child;
                    break;
                  }
                }

                if (apply_func)
                {
                  auto params = apply_func / Params;
                  auto when_args = stmt / Args;
                  for (size_t i = 1;
                       i < when_args->size() && i < params->size(); ++i)
                  {
                    auto arg_it =
                      env.find((when_args->at(i) / Rhs)->location());
                    if (arg_it == env.end())
                      continue;
                    auto ci = extract_cown_inner(arg_it->second.type);
                    if (!ci)
                      continue;
                    auto new_type = ref_type(ci);
                    auto param = params->at(i);
                    param->replace(param / Type, new_type);
                    env[(param / Ident)->location()] =
                      {clone(new_type), true, {}};
                  }
                }
              }
            }
          }
        }
      }
      // ----- Typetest -----
      else if (stmt == Typetest)
      {
        merge(
          (stmt / LocalId)->location(), primitive_type(Bool));
      }
    }

    // Finalize tuple/array-lit types within this label.
    for (auto& [loc, tt] : tuple_locals)
    {
      if (tt.is_array_lit)
      {
        // Sibling refinement: find dominant non-default type.
        Node dom_prim;
        for (size_t i = 0; i < tt.size; i++)
        {
          if (tt.element_value_locs[i].view().empty())
            continue;
          auto it = env.find(tt.element_value_locs[i]);
          if (it == env.end() || is_default_type(it->second.type))
            continue;
          dom_prim = extract_primitive(it->second.type);
          if (dom_prim)
            break;
        }
        if (dom_prim)
        {
          auto dom_type = primitive_type(dom_prim->type());
          for (size_t i = 0; i < tt.size; i++)
          {
            if (!tt.element_value_locs[i].view().empty())
              merge(tt.element_value_locs[i], dom_type);
          }
        }

        // Determine homogeneous type.
        Node common;
        bool uniform = true;
        for (size_t i = 0; i < tt.size && uniform; i++)
        {
          if (tt.element_value_locs[i].view().empty())
          {
            uniform = false;
            break;
          }
          auto it = env.find(tt.element_value_locs[i]);
          if (it == env.end() || !extract_primitive(it->second.type))
          {
            uniform = false;
            break;
          }
          if (!common)
            common = clone(it->second.type);
          else if (
            it->second.type->front()->type() != common->front()->type())
            uniform = false;
        }
        if (uniform && common && tt.size > 0)
          env[loc] = {clone(common), false, {}};
      }
      else
      {
        // Heterogeneous tuple.
        bool complete = true;
        for (size_t i = 0; i < tt.size && complete; i++)
          if (!tt.element_types[i])
            complete = false;
        if (complete && tt.size > 0)
        {
          if (tt.size == 1)
            env[loc] = {
              Type << clone(tt.element_types[0]->front()), false, {}};
          else
          {
            Node tup = TupleType;
            for (auto& et : tt.element_types)
              tup << clone(et->front());
            env[loc] = {Type << clone(tup), false, {}};
          }
        }
      }
    }

    return changes;
  }

  // ===== Fixpoint algorithm =====

  static bool process_function(Node node, Node top, bool check_errors)
  {
    auto labels = node / Labels;
    size_t n = labels->size();
    if (n == 0)
      return true;

    // Build label graph.
    std::map<std::string, size_t> label_idx;
    for (size_t i = 0; i < n; i++)
      label_idx[std::string((labels->at(i) / LabelId)->location().view())] = i;

    std::vector<std::vector<size_t>> succ(n), pred(n);
    for (size_t i = 0; i < n; i++)
    {
      auto term = labels->at(i) / Return;
      if (term == Cond)
      {
        auto t = label_idx.find(std::string((term / Lhs)->location().view()));
        auto f = label_idx.find(std::string((term / Rhs)->location().view()));
        if (t != label_idx.end())
          succ[i].push_back(t->second);
        if (f != label_idx.end())
          succ[i].push_back(f->second);
      }
      else if (term == Jump)
      {
        auto t =
          label_idx.find(std::string((term / LabelId)->location().view()));
        if (t != label_idx.end())
          succ[i].push_back(t->second);
      }
    }
    for (size_t i = 0; i < n; i++)
      for (auto s : succ[i])
        pred[s].push_back(i);

    // Initialize exit envs.
    std::vector<TypeEnv> exit_envs(n);
    std::map<std::pair<size_t, size_t>, TypeEnv> branch_exits;

    for (auto& pd : *(node / Params))
    {
      auto type = pd / Type;
      bool fixed = type->front() != TypeVar;
      exit_envs[0][(pd / Ident)->location()] =
        {clone(type), fixed, {}};
    }

    // Backward envs: carry type expectations from downstream.
    std::vector<TypeEnv> bwd_envs(n);

    // Initialize backward env for Return labels with declared return type.
    {
      auto func_ret = node / Type;
      if (!contains_typevar(func_ret) && !is_default_type(func_ret))
      {
        for (size_t j = 0; j < n; j++)
        {
          auto term = labels->at(j) / Return;
          if (term == Return)
          {
            auto ret_loc = (term / LocalId)->location();
            bwd_envs[j][ret_loc] = {clone(func_ret), false, {}};
          }
        }
      }
    }

    // Shared state.
    std::map<Location, Node> lookup_stmts;
    std::vector<std::pair<Location, Location>> typevar_aliases;
    std::map<Location, std::pair<Location, size_t>> ref_to_tuple;

    // Worklist algorithm.
    std::set<size_t> worklist;
    for (size_t i = 0; i < n; i++)
      worklist.insert(i);

    size_t wl_iters = 0;
    while (!worklist.empty())
    {
      wl_iters++;
      if (wl_iters > n * 100)
        break;

      auto wit = worklist.begin();
      size_t i = *wit;
      worklist.erase(wit);

      // Build entry from predecessor exit envs.
      TypeEnv entry;
      if (i == 0)
      {
        for (auto& [loc, info] : exit_envs[0])
          entry[loc] = {info.type, info.is_fixed, info.call_node};
      }
      else
      {
        for (auto p : pred[i])
        {
          auto bk = std::make_pair(p, i);
          auto bi = branch_exits.find(bk);
          const auto& pe = (bi != branch_exits.end()) ? bi->second
                                                       : exit_envs[p];
          for (auto& [loc, info] : pe)
          {
            auto eit = entry.find(loc);
            if (eit == entry.end())
              entry[loc] = {info.type, info.is_fixed, info.call_node};
            else
            {
              auto m = merge_type(eit->second.type, info.type, top);
              if (m)
                eit->second.type = m;
              if (!eit->second.call_node && info.call_node)
                eit->second.call_node = info.call_node;
            }
          }
        }
      }

      // Merge backward envs for this label and its successors into entry.
      // Including bwd_envs[i] lets same-label requeueing expose new local
      // backward facts to earlier statements on the next iteration.
      for (auto& [loc, info] : bwd_envs[i])
      {
        auto eit = entry.find(loc);
        if (eit == entry.end())
          continue;
        auto m = merge_type(eit->second.type, info.type, top);
        if (m)
          eit->second.type = m;
      }

      // Merge backward envs from successors into entry.
      for (auto s : succ[i])
      {
        for (auto& [loc, info] : bwd_envs[s])
        {
          auto eit = entry.find(loc);
          if (eit == entry.end())
            continue;
          auto m = merge_type(eit->second.type, info.type, top);
          if (m)
            eit->second.type = m;
        }
      }

      // Copy entry → working env.
      TypeEnv env;
      for (auto& [loc, info] : entry)
        env[loc] = {info.type, info.is_fixed, info.call_node};

      // Per-label state.
      std::map<Location, TupleTracking> tuple_locals;

      // Process body.
      auto label_changes = process_label_body(
        labels->at(i) / Body, env, bwd_envs[i], top, lookup_stmts,
        typevar_aliases, ref_to_tuple, tuple_locals);
      bool label_changed = label_changes.forward;

      // Return terminator: backward merge.
      auto term = labels->at(i) / Return;
      if (term == Return)
      {
        auto func_ret = node / Type;
        if (func_ret->front() != TypeVar && !is_default_type(func_ret))
        {
          auto ret_loc = (term / LocalId)->location();
          if (merge_env(env, ret_loc, clone(func_ret), top))
          {
            propagate_call_node(env, ret_loc, top, lookup_stmts);
            label_changed = true;
            bwd_envs[i][ret_loc] = {clone(func_ret), false, {}};
          }
        }
      }

      // Cond: branch exits with typetest narrowing.
      if (term == Cond)
      {
        auto trace = trace_typetest(term / LocalId, labels->at(i) / Body);
        if (trace)
        {
          auto t_it =
            label_idx.find(std::string((term / Lhs)->location().view()));
          auto f_it =
            label_idx.find(std::string((term / Rhs)->location().view()));

          auto make_clone = [&]() {
            TypeEnv c;
            for (auto& [loc, info] : env)
              c[loc] = {clone(info.type), info.is_fixed, info.call_node};
            return c;
          };

          if (!trace->negated)
          {
            if (t_it != label_idx.end())
            {
              auto ne = make_clone();
              ne[trace->src->location()] =
                {clone(trace->type), true, {}};
              branch_exits[{i, t_it->second}] = std::move(ne);
            }
            if (f_it != label_idx.end())
              branch_exits[{i, f_it->second}] = make_clone();
          }
          else
          {
            if (f_it != label_idx.end())
            {
              auto ne = make_clone();
              ne[trace->src->location()] =
                {clone(trace->type), true, {}};
              branch_exits[{i, f_it->second}] = std::move(ne);
            }
            if (t_it != label_idx.end())
              branch_exits[{i, t_it->second}] = make_clone();
          }
        }
      }

      // Update forward exit env. Re-queue successors if forward changed.
      exit_envs[i] = std::move(env);
      if (label_changed)
      {
        for (auto s : succ[i])
          worklist.insert(s);
      }

      // Body-local backward refinement can improve earlier statements in the
      // same label (for example, a later comparison refining an earlier
      // CallDyn result), so re-run this label until its local backward facts
      // stabilize.
      if (label_changes.backward)
        worklist.insert(i);

      // Propagate backward constraints from successors.
      bool bwd_changed = label_changes.backward;
      for (auto s : succ[i])
      {
        for (auto& [loc, info] : bwd_envs[s])
        {
          auto bit = bwd_envs[i].find(loc);
          if (bit == bwd_envs[i].end())
          {
            bwd_envs[i][loc] = {info.type, false, {}};
            bwd_changed = true;
          }
          else
          {
            auto m = merge_type(bit->second.type, info.type, top);
            if (m)
            {
              bit->second.type = m;
              bwd_changed = true;
            }
          }
        }
      }

      // Re-queue predecessors if backward changed.
      if (bwd_changed)
      {
        for (auto p : pred[i])
          worklist.insert(p);
      }
    }

    for (size_t i = 0; i < n; i++)
      run_dependency_cascade(
        exit_envs[i], labels->at(i) / Body, top, lookup_stmts);

    // ===== Finalization =====

    // 1. Consts: write inferred types to AST.
    for (size_t i = 0; i < n; i++)
    {
      auto body = labels->at(i) / Body;
      auto it = body->begin();
      while (it != body->end())
      {
        // 2. TypeAssertion: remove.
        if (*it == TypeAssertion)
        {
          it = body->erase(it, std::next(it));
          continue;
        }

        if (*it == Const)
        {
          auto dst = (*it)->front();
          auto loc = dst->location();
          auto env_it = exit_envs[i].find(loc);
          Node final_prim;
          if (env_it != exit_envs[i].end() &&
              !is_default_type(env_it->second.type))
            final_prim = extract_primitive(env_it->second.type);

          if (final_prim)
          {
            if ((*it)->size() == 3)
            {
              auto old_type = (*it)->at(1);
              if (old_type->type() != final_prim->type())
                (*it)->replace(old_type, final_prim->type());
            }
            else if ((*it)->size() == 2)
            {
              auto lit = (*it)->back();
              (*it)->erase((*it)->begin(), (*it)->end());
              *it << dst << final_prim->type() << lit;
            }
          }
          else if ((*it)->size() == 2)
          {
            auto lit = (*it)->back();
            auto type_tok = default_literal_type(lit);
            (*it)->erase((*it)->begin(), (*it)->end());
            *it << dst << type_tok << lit;
          }
        }
        // 3. NewArrayConst: update Type from env.
        else if (*it == NewArrayConst)
        {
          auto nloc = ((*it) / LocalId)->location();
          auto env_it = exit_envs[i].find(nloc);
          if (env_it != exit_envs[i].end())
          {
            auto inner = env_it->second.type->front();
            if (inner == TupleType)
              (*it)->replace((*it) / Type, clone(env_it->second.type));
            else
            {
              auto prim = extract_primitive(env_it->second.type);
              if (prim && !Subtype.invariant(
                            top, (*it) / Type, env_it->second.type))
                (*it)->replace((*it) / Type, clone(env_it->second.type));
            }
          }
        }

        ++it;
      }
    }

    // 5. TypeVar back-prop.
    {
      bool tv_changed = true;
      while (tv_changed)
      {
        tv_changed = false;
        for (auto& [dst_loc, src_loc] : typevar_aliases)
        {
          for (auto& ee : exit_envs)
          {
            auto d = ee.find(dst_loc);
            auto s = ee.find(src_loc);
            if (d == ee.end() || s == ee.end())
              continue;
            bool dtv = d->second.type->front() == TypeVar;
            bool stv = s->second.type->front() == TypeVar;
            if (!dtv && stv)
            {
              s->second.type = clone(d->second.type);
              s->second.is_fixed = true;
              tv_changed = true;
            }
            else if (dtv && !stv)
            {
              d->second.type = clone(s->second.type);
              tv_changed = true;
            }
          }
        }
      }
    }

    // 6. Update params and fields.
    auto parent_cls = node->parent(ClassDef);
    for (auto& pd : *(node / Params))
    {
      auto type = pd / Type;
      if (type->front() != TypeVar)
        continue;
      auto ident = pd / Ident;
      for (auto& ee : exit_envs)
      {
        auto it = ee.find(ident->location());
        if (it != ee.end() && it->second.type->front() != TypeVar)
        {
          pd->replace(type, clone(it->second.type));
          if (parent_cls)
          {
            for (auto& child : *(parent_cls / ClassBody))
            {
              if (child != FieldDef)
                continue;
              if ((child / Ident)->location().view() !=
                  ident->location().view())
                continue;
              if (contains_typevar(child / Type))
                child->replace(child / Type, clone(it->second.type));
              break;
            }
          }
          break;
        }
      }
    }

    if (parent_cls)
    {
      for (auto& child : *(parent_cls / ClassBody))
      {
        if (child != FieldDef || (child / Type)->front() != TypeVar)
          continue;
        auto fname = (child / Ident)->location();
        for (auto& ee : exit_envs)
        {
          auto it = ee.find(fname);
          if (it != ee.end() && it->second.type->front() != TypeVar)
          {
            child->replace(child / Type, clone(it->second.type));
            break;
          }
        }
      }
    }

    // 4. Return type inference.
    bool in_generic = (node / TypeParams)->size() > 0 ||
      (node->parent({ClassDef}) != nullptr &&
       (node->parent({ClassDef}) / TypeParams)->size() > 0);

    auto func_ret = node / Type;
    if (func_ret->front() == TypeVar)
    {
      SequentCtx ctx{top, {}, {}};
      Nodes ret_types;
      bool unresolved = false;

      if (in_generic)
      {
        for (auto& lbl : *labels)
          for (auto& stmt : *(lbl / Body))
            if (stmt->in({CallDyn, TryCallDyn}))
            {
              auto loc = (stmt / LocalId)->location();
              bool found = false;
              for (auto& ee : exit_envs)
                if (ee.find(loc) != ee.end())
                {
                  found = true;
                  break;
                }
              if (!found)
                unresolved = true;
            }
      }

      for (size_t i = 0; i < n; i++)
      {
        auto term = labels->at(i) / Return;
        if (term != Return)
          continue;
        auto ret_loc = (term / LocalId)->location();
        auto eit = exit_envs[i].find(ret_loc);
        if (eit == exit_envs[i].end() || eit->second.type->front() == TypeVar)
        {
          if (in_generic)
            unresolved = true;
          continue;
        }
        bool covered = false;
        for (auto& rt : ret_types)
          if (Subtype(ctx, eit->second.type, rt))
          {
            covered = true;
            break;
          }
        if (!covered)
          ret_types.push_back(clone(eit->second.type));
      }

      if (!unresolved)
      {
        if (ret_types.size() == 1)
          node->replace(func_ret, ret_types.front());
        else if (ret_types.size() > 1)
        {
          Node u = Union;
          for (auto& rt : ret_types)
            u << clone(rt->front());
          node->replace(func_ret, Type << u);
        }
        else
        {
          bool all_nonlocal = true;
          for (auto& lbl : *labels)
          {
            auto term = lbl / Return;
            if (term->in({Jump, Cond}))
              continue;
            if (term != Raise)
            {
              all_nonlocal = false;
              break;
            }
          }
          if (all_nonlocal)
            node->replace(
              func_ret,
              Type
                << (TypeName
                      << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                      << (NameElement << (Ident ^ "none") << TypeArgs)));
        }
      }
    }

    // Error checking.
    if (check_errors && !in_generic)
    {
      for (auto& pd : *(node / Params))
      {
        if ((pd / Type)->front() == TypeVar)
        {
          node->parent()->replace(
            node, err(pd / Ident, "Cannot infer type of parameter"));
          return false;
        }
      }
      func_ret = node / Type;
      if (func_ret->front() == TypeVar)
      {
        node->parent()->replace(
          node, err(node / Ident, "Cannot infer return type of function"));
        return false;
      }
    }
    return true;
  }

  // ===== has_typevar (simplified) =====

  static bool has_typevar(Node func)
  {
    for (auto& pd : *(func / Params))
      if ((pd / Type)->front() == TypeVar)
        return true;
    auto ret = (func / Type)->front();
    if (ret == TypeVar || ret->in({DefaultInt, DefaultFloat}))
      return true;
    if (ret == Union)
      for (auto& child : *ret)
        if (child->in({DefaultInt, DefaultFloat}))
          return true;
    return false;
  }

  // ===== Pass definition =====

  PassDef infer()
  {
    PassDef p{"infer", wfPassInfer, dir::once, {}};

    p.post([](auto top) {
      Nodes deferred;

      top->traverse([&](auto node) {
        if (node != Function)
          return node == Top || node == ClassDef || node == ClassBody ||
            node == Lib || node == Symbols;
        process_function(node, top, false);
        if (has_typevar(node))
          deferred.push_back(node);
        return false;
      });

      size_t prev = deferred.size();
      for (size_t iter = 0; iter < deferred.size(); iter++)
      {
        for (auto& func : deferred)
        {
          if (has_typevar(func))
            func->replace(func / Type, make_type());
          process_function(func, top, false);
        }
        size_t count = 0;
        for (auto& func : deferred)
          if (has_typevar(func))
            count++;
        if (count == 0 || count >= prev)
          break;
        prev = count;
      }

      for (auto& func : deferred)
        if (has_typevar(func))
          process_function(func, top, true);

      // Sweep: DefaultInt → u64, DefaultFloat → f64.
      top->traverse([](Node& node) {
        if (node->in({DefaultInt, DefaultFloat}))
        {
          auto parent = node->parent();
          bool is_int = (node == DefaultInt);
          if (parent == Const)
            parent->replace(node, is_int ? Node{U64} : Node{F64});
          else
            parent->replace(
              node, is_int ? primitive_type(U64)->front()
                           : primitive_type(F64)->front());
          return false;
        }
        return true;
      });

      return 0;
    });

    return p;
  }
}
