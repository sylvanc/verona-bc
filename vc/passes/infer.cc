#include "../lang.h"
#include "../subtype.h"

#include <queue>

namespace vc
{
  // Map from builtin primitive name to IR token.
  const std::map<std::string_view, Token> primitive_from_name = {
    {"none", None},
    {"bool", Bool},
    {"i8", I8},
    {"i16", I16},
    {"i32", I32},
    {"i64", I64},
    {"u8", U8},
    {"u16", U16},
    {"u32", U32},
    {"u64", U64},
    {"ilong", ILong},
    {"ulong", ULong},
    {"isize", ISize},
    {"usize", USize},
    {"f32", F32},
    {"f64", F64},
  };

  // Map from ffi primitive name to IR token.
  // These live under _builtin::ffi:: (3-element path).
  const std::map<std::string_view, Token> ffi_primitive_from_name = {
    {"ptr", Ptr},
    {"callback", Callback},
  };

  const std::initializer_list<Token> integer_types = {
    I8, I16, I32, I64, U8, U16, U32, U64, ILong, ULong, ISize, USize};

  const std::initializer_list<Token> float_types = {F32, F64};

  // Build a source-level Type node wrapping a FQ TypeName for a primitive.
  // Creates fresh nodes on each call (no shared-node issues).
  Node primitive_type(const Token& tok)
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ tok.str()) << TypeArgs));
  }

  // Build a source-level Type node for an ffi primitive (_builtin::ffi::name).
  // Creates fresh nodes on each call.
  Node ffi_primitive_type(const Token& tok)
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "ffi") << TypeArgs)
                   << (NameElement << (Ident ^ tok.str()) << TypeArgs));
  }

  // Build a source-level Type node for _builtin::array[_builtin::u8].
  // Creates fresh nodes on each call.
  Node string_type()
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "string") << TypeArgs));
  }

  // Build a source-level Type node for _builtin::ref[inner].
  // Creates fresh nodes on each call.
  Node ref_type(const Node& inner_type)
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "ref")
                                   << (TypeArgs << clone(inner_type))));
  }

  // Build a source-level Type node for _builtin::cown[inner].
  // Creates fresh nodes on each call.
  Node cown_type(const Node& inner_type)
  {
    return Type
      << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                   << (NameElement << (Ident ^ "cown")
                                   << (TypeArgs << clone(inner_type))));
  }

  // If type_node is _builtin::X[T] for the given wrapper name,
  // return T as a cloned Type node. Returns empty Node otherwise.
  Node extract_wrapper_inner(const Node& type_node, std::string_view wrapper)
  {
    if (type_node != Type)
      return {};

    auto inner = type_node->front();

    if (inner != TypeName || inner->size() != 2)
      return {};

    auto first = (inner->front() / Ident)->location().view();
    auto second = (inner->back() / Ident)->location().view();

    if (first != "_builtin" || second != wrapper)
      return {};

    auto ta = inner->back() / TypeArgs;

    if (ta->size() != 1)
      return {};

    return clone(ta->front());
  }

  // If type_node is _builtin::ref[T], return T as a cloned Type node.
  // Returns empty Node otherwise.
  Node extract_ref_inner(const Node& type_node)
  {
    return extract_wrapper_inner(type_node, "ref");
  }

  // If type_node is _builtin::cown[T], return T as a cloned Type node.
  // Returns empty Node otherwise.
  Node extract_cown_inner(const Node& type_node)
  {
    return extract_wrapper_inner(type_node, "cown");
  }

  // Check if a Type node directly references a single TypeParam.
  // Returns the TypeParam def node, or empty Node.
  Node direct_typeparam(Node top, const Node& type_node)
  {
    if (type_node != Type)
      return {};

    auto inner = type_node->front();

    if (inner != TypeName)
      return {};

    auto def = find_def(top, inner);

    if (def && (def == TypeParam))
      return def;

    return {};
  }

  // Entry for a local variable in the type environment.
  struct LocalTypeInfo
  {
    Node type; // Source-level Type node (e.g., Type << TypeName << ...)
    bool is_default; // True if type came from Const default (U64/F64)
    bool is_fixed; // True if from Param or TypeAssertion
    Node const_node; // If from a Const, the Const stmt (for refinement)
    Node call_node; // If from a default-inferred Call/CallDyn, the stmt

    // Factory: fixed type from Param or TypeAssertion.
    static LocalTypeInfo fixed(Node type)
    {
      return {std::move(type), false, true, {}, {}};
    }

    // Factory: default-typed literal (U64/F64).
    static LocalTypeInfo literal(Node type, Node const_node)
    {
      return {std::move(type), true, false, std::move(const_node), {}};
    }

    // Factory: computed result (non-default, non-fixed).
    static LocalTypeInfo computed(Node type)
    {
      return {std::move(type), false, false, {}, {}};
    }

    // Factory: propagated from another entry (Copy/Move).
    static LocalTypeInfo propagated(const LocalTypeInfo& src)
    {
      return {
        clone(src.type), src.is_default, false, src.const_node, src.call_node};
    }
  };

  using TypeEnv = std::map<Location, LocalTypeInfo>;

  // Forward declaration for use in extract_constraints.
  Node apply_subst(Node top, const Node& type_node, const NodeMap<Node>& subst);

  // Build a substitution map from a ClassDef's TypeParams to a TypeName's
  // last NameElement's TypeArgs. Returns an empty map on size mismatch.
  NodeMap<Node>
  build_class_subst(const Node& class_def, const Node& typename_node)
  {
    NodeMap<Node> subst;
    auto tps = class_def / TypeParams;
    auto ta = typename_node->back() / TypeArgs;

    if (tps->size() == ta->size())
    {
      for (size_t i = 0; i < tps->size(); i++)
        subst[tps->at(i)] = ta->at(i);
    }

    return subst;
  }

  // Extract TypeParam constraints by structurally matching a formal type
  // against an actual type. For example, formal = wrapper[T] matched
  // against actual = wrapper[i32] yields constraint TypeParam(T) -> i32.
  // Handles algebraic types (Union, Isect, TupleType) element-wise.
  void extract_constraints(
    Node top,
    const Node& f_inner, // wfType node from formal type
    const Node& a_inner, // wfType node from actual type
    NodeMap<LocalTypeInfo>& constraints,
    bool is_default)
  {
    // Case 1: formal is a TypeParam reference.
    if (f_inner == TypeName)
    {
      auto def = find_def(top, f_inner);

      if (def && (def == TypeParam))
      {
        Node actual_type = Type << clone(a_inner);
        auto existing = constraints.find(def);

        if (existing == constraints.end())
        {
          constraints[def] = {actual_type, is_default, false, {}, {}};
        }
        else if (existing->second.is_default && !is_default)
        {
          existing->second = LocalTypeInfo::computed(actual_type);
        }

        return;
      }
    }

    // Case 2: both TypeNames - match structurally by ident path,
    // or via shape method return types if the formal is a shape.
    if ((f_inner == TypeName) && (a_inner == TypeName))
    {
      // Phase 1: Check if the FQ paths match structurally.
      bool structural_match = (f_inner->size() == a_inner->size());

      if (structural_match)
      {
        for (size_t i = 0; i < f_inner->size(); i++)
        {
          auto f_elem = f_inner->at(i);
          auto a_elem = a_inner->at(i);

          if (
            (f_elem / Ident)->location().view() !=
            (a_elem / Ident)->location().view())
          {
            structural_match = false;
            break;
          }

          if ((f_elem / TypeArgs)->size() != (a_elem / TypeArgs)->size())
          {
            structural_match = false;
            break;
          }
        }
      }

      if (structural_match)
      {
        // Paths match: extract constraints from TypeArgs at each element.
        for (size_t i = 0; i < f_inner->size(); i++)
        {
          auto f_ta = f_inner->at(i) / TypeArgs;
          auto a_ta = a_inner->at(i) / TypeArgs;

          for (size_t j = 0; j < f_ta->size(); j++)
          {
            extract_constraints(
              top,
              f_ta->at(j)->front(),
              a_ta->at(j)->front(),
              constraints,
              is_default);
          }
        }

        return;
      }

      // Phase 2: Shape-aware matching.
      // When the formal resolves to a shape and the actual to a class,
      // extract constraints by matching shape method return types against
      // actual class method return types. E.g., formal = iterator[T] vs
      // actual = range[i32]: iterator::next returns T, range::next
      // returns i32, so T -> i32.
      auto f_def = find_def(top, f_inner);
      auto a_def = find_def(top, a_inner);

      if (
        f_def && a_def && (f_def == ClassDef) && ((f_def / Shape) == Shape) &&
        (a_def == ClassDef))
      {
        // Map shape TypeParams -> formal TypeArgs (which reference the
        // caller's TypeParams, e.g., iterator.A -> chain.A).
        auto shape_to_formal = build_class_subst(f_def, f_inner);

        if (!shape_to_formal.empty())
        {
          // Map actual class TypeParams -> actual TypeArgs (concrete
          // types, e.g., range.A -> i32). Empty for non-generic
          // actual classes, which is fine — apply_subst clones as-is.
          auto actual_subst = build_class_subst(a_def, a_inner);

          // For each shape method, find matching actual method and
          // extract constraints from the return types.
          auto shape_body = f_def / ClassBody;
          auto actual_body = a_def / ClassBody;

          for (auto& shape_func : *shape_body)
          {
            if (shape_func != Function)
              continue;

            auto shape_ret = shape_func / Type;
            auto method_name = (shape_func / Ident)->location().view();
            auto hand = (shape_func / Lhs)->type();
            auto arity = (shape_func / Params)->size();

            for (auto& actual_func : *actual_body)
            {
              if (actual_func != Function)
                continue;

              if ((actual_func / Ident)->location().view() != method_name)
                continue;

              if ((actual_func / Lhs)->type() != hand)
                continue;

              if ((actual_func / Params)->size() != arity)
                continue;

              // Substitute shape TypeParams with the formal TypeArgs
              // (caller's TypeParam refs). E.g., iterator.A becomes
              // chain.A in the substituted return type.
              auto formal_ret = apply_subst(top, shape_ret, shape_to_formal);

              // Substitute actual TypeParams with concrete TypeArgs.
              // E.g., range.A becomes i32.
              auto actual_ret =
                apply_subst(top, actual_func / Type, actual_subst);

              // Recursively extract constraints between the
              // transformed return types.
              extract_constraints(
                top,
                formal_ret->front(),
                actual_ret->front(),
                constraints,
                is_default);

              break;
            }
          }
        }
      }

      return;
    }

    // Case 3: Union/Isect/TupleType - element-wise matching.
    if (
      (f_inner->type() == a_inner->type()) &&
      (f_inner->size() == a_inner->size()) &&
      f_inner->in({Union, Isect, TupleType}))
    {
      for (size_t i = 0; i < f_inner->size(); i++)
      {
        // Children are wfType directly.
        extract_constraints(
          top, f_inner->at(i), a_inner->at(i), constraints, is_default);
      }

      return;
    }
  }

  // Apply a TypeParam substitution to a Type node, returning a new Type
  // with all TypeParam references resolved.
  Node apply_subst(Node top, const Node& type_node, const NodeMap<Node>& subst)
  {
    if (type_node != Type)
      return clone(type_node);

    if (subst.empty())
      return clone(type_node);

    auto inner = type_node->front();

    if (inner == TypeName)
    {
      // Check if this TypeName resolves to a TypeParam in the subst.
      auto def = find_def(top, inner);

      if (def && (def == TypeParam))
      {
        auto it = subst.find(def);

        if (it != subst.end())
          return clone(it->second);
      }

      // Not a TypeParam - recurse into TypeArgs of each NameElement.
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
      {
        // Children are wfType, wrap in Type for recursive call.
        Node wrapped = Type << clone(child);
        Node substituted = apply_subst(top, wrapped, subst);
        new_inner << substituted->front();
      }

      return Type << new_inner;
    }

    // TypeVar, TypeSelf, etc. - return as-is.
    return clone(type_node);
  }

  // Propagate method signatures from a shape type to a lambda class.
  // When a formal param has shape type (e.g., fn$5[i32]) and the actual
  // arg is a lambda class (e.g., lambda$0), propagate the shape's method
  // param types and return type to the lambda's matching methods.
  // This enables type inference for lambda parameters when the lambda is
  // passed to a higher-order function like `each` or `pairs`.
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
    auto actual_subst = build_class_subst(actual_def, actual_inner);
    auto shape_body = shape_def / ClassBody;
    auto actual_body = actual_def / ClassBody;

    for (auto& shape_func : *shape_body)
    {
      if (shape_func != Function)
        continue;

      auto method_name = (shape_func / Ident)->location().view();
      auto hand = (shape_func / Lhs)->type();
      auto shape_params = shape_func / Params;
      auto shape_ret = shape_func / Type;

      for (auto& actual_func : *actual_body)
      {
        if (actual_func != Function)
          continue;

        if ((actual_func / Ident)->location().view() != method_name)
          continue;

        if ((actual_func / Lhs)->type() != hand)
          continue;

        if ((actual_func / Params)->size() != shape_params->size())
          continue;

        // Found matching method. Propagate param types.
        auto actual_params = actual_func / Params;

        for (size_t j = 0; j < shape_params->size(); j++)
        {
          auto actual_param = actual_params->at(j);
          auto actual_param_type = actual_param / Type;

          if (actual_param_type->front() != TypeVar)
            continue;

          auto shape_param_type =
            apply_subst(top, shape_params->at(j) / Type, shape_subst);

          if (!shape_param_type || shape_param_type->front() == TypeVar)
            continue;

          // Skip TypeSelf — the shape's self param type is not useful.
          if (shape_param_type->front() == TypeSelf)
            continue;

          actual_param->replace(actual_param_type, clone(shape_param_type));
        }

        // Propagate return type.
        auto actual_ret = actual_func / Type;

        if (actual_ret->front() == TypeVar)
        {
          auto shape_ret_subst = apply_subst(top, shape_ret, shape_subst);

          if (shape_ret_subst && shape_ret_subst->front() != TypeVar)
            actual_func->replace(actual_ret, clone(shape_ret_subst));
        }

        break;
      }
    }
  }

  // Resolve the return type of a method on a receiver type.
  // Given a receiver type (e.g., wrapper[i32]), a method name, hand,
  // arity, and method-level TypeArgs, find the method definition
  // and return the substituted return type.
  // Information about a resolved method.
  struct MethodInfo
  {
    Node func; // The matched Function node.
    NodeMap<Node> subst; // Combined class + function TypeParam substitutions.
  };

  // Find a method on a class by receiver type, name, hand, and arity.
  // Returns the Function node and substitution map, or empty MethodInfo
  // if not found. If the receiver is ref[T] or cown[T] and the method
  // is not found on the wrapper, tries the inner type T.
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

    if (!class_def || (class_def != ClassDef))
      return {};

    // Build substitution from class-level TypeArgs.
    auto subst = build_class_subst(class_def, inner);

    // Find the matching method in the class.
    auto class_body = class_def / ClassBody;
    auto method_name = method_ident->location().view();

    for (auto& child : *class_body)
    {
      if (child != Function)
        continue;

      if ((child / Ident)->location().view() != method_name)
        continue;

      auto child_hand = (child / Lhs)->type();

      if (child_hand != hand)
        continue;

      if ((child / Params)->size() != arity)
        continue;

      // Found the method. Add function-level TypeParam substitutions.
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

    // Method not found on the direct type. If this is ref[T] or cown[T],
    // try the inner type T (auto-deref for method resolution).
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

    // If the return type is TypeVar (e.g., auto-generated rhs wrapper
    // not yet processed), compute the return type from the lhs
    // counterpart: the rhs wrapper calls the lhs version (via a Call
    // with Lhs hand) and does a Load, so the return type is the lhs
    // version's return type with ref unwrapped.
    if (ret && ret->front() == TypeVar && hand == Rhs)
    {
      auto lhs_info = resolve_method(
        top, receiver_type, method_ident, Lhs, arity, method_typeargs);

      if (lhs_info.func)
      {
        auto lhs_ret = apply_subst(top, lhs_info.func / Type, lhs_info.subst);
        auto inner = extract_ref_inner(lhs_ret);

        if (inner)
          return inner;
      }
    }

    return ret;
  }

  // Extract the primitive token node from a source-level Type node that
  // references a _builtin primitive. Returns empty Node if not a
  // primitive.
  Node extract_primitive(const Node& type_node)
  {
    if (type_node != Type)
      return {};

    auto inner = type_node->front();

    if (inner != TypeName)
      return {};

    auto first_ident = (inner->front() / Ident)->location().view();

    if (first_ident != "_builtin")
      return {};

    // Two-element path: _builtin::name (flat primitives).
    if (inner->size() == 2)
    {
      auto second_ident = (inner->back() / Ident)->location().view();
      auto it = primitive_from_name.find(second_ident);

      if (it != primitive_from_name.end())
        return it->second;

      return {};
    }

    // Three-element path: _builtin::ffi::name (ffi primitives).
    if (inner->size() == 3)
    {
      auto second_ident = (inner->at(1) / Ident)->location().view();

      if (second_ident != "ffi")
        return {};

      auto third_ident = (inner->at(2) / Ident)->location().view();
      auto it = ffi_primitive_from_name.find(third_ident);

      if (it != ffi_primitive_from_name.end())
        return it->second;

      return {};
    }

    return {};
  }

  // Like extract_primitive but also handles union and intersection
  // types by scanning all components for primitives. Returns empty
  // if ambiguous (components with different primitive types found).
  // Compare two Type nodes for equality by primitive token.
  // For primitives, compares the _builtin type name.
  // For non-primitives, falls back to comparing inner Token types
  // and (for TypeName) the last NameElement's ident.
  bool types_equal(const Node& a, const Node& b)
  {
    if (a != Type || b != Type)
      return false;

    auto ia = a->front();
    auto ib = b->front();

    // Different inner Token types → not equal.
    if (ia->type() != ib->type())
      return false;

    // Both are TypeName: compare the fully-qualified name + TypeArgs.
    if (ia == TypeName && ib == TypeName)
    {
      if (ia->size() != ib->size())
        return false;

      for (size_t i = 0; i < ia->size(); i++)
      {
        auto ea = ia->at(i) / Ident;
        auto eb = ib->at(i) / Ident;

        if (ea->location().view() != eb->location().view())
          return false;

        auto ta_a = ia->at(i) / TypeArgs;
        auto ta_b = ib->at(i) / TypeArgs;

        if (ta_a->size() != ta_b->size())
          return false;

        for (size_t j = 0; j < ta_a->size(); j++)
        {
          if (!types_equal(ta_a->at(j), ta_b->at(j)))
            return false;
        }
      }

      return true;
    }

    // Same Token type (TypeVar, Dyn, etc.) — treat as equal.
    return true;
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
      Node wrapped = Type << clone(component);
      auto p = extract_primitive(wrapped);

      if (!p)
        continue;

      if (candidate && candidate->type() != p->type())
        return {}; // Different primitive types — ambiguous.

      candidate = p;
    }

    return candidate;
  }

  // Extract primitive from ref[T] or cown[T] wrapper's inner type.
  Node extract_wrapper_primitive(const Node& type_node)
  {
    auto inner = extract_ref_inner(type_node);

    if (!inner)
      inner = extract_cown_inner(type_node);

    if (inner)
      return extract_callable_primitive(inner);

    return {};
  }

  // Return the default primitive type for a literal node.
  Node default_literal_type(const Node& lit)
  {
    if (lit->in({True, False}))
      return Bool;

    if (lit == None)
      return None;

    if (lit->in({Bin, Oct, Int, Hex, Char}))
      return U64;

    if (lit->in({Float, HexFloat}))
      return F64;

    assert(false && "unhandled literal type in infer");
    return {};
  }

  // Refine a Const node's type to new_prim. Updates env entries sharing
  // the same const_node. Does NOT mutate the AST — Const types are
  // written to the AST once after convergence.
  void refine_const(TypeEnv& env, Node const_node, const Token& new_prim)
  {
    assert(const_node == Const);

    // Update all env entries referencing this Const.
    Node new_src_type = primitive_type(new_prim);

    for (auto& [loc, info] : env)
    {
      if (info.const_node == const_node)
      {
        info.type = clone(new_src_type);
        info.is_default = false;
        info.const_node = {};
      }
    }
  }

  // Try to refine a default-typed Const at the given location to match
  // expected_prim. Returns true if refinement occurred.
  bool try_refine(TypeEnv& env, const Location& loc, const Node& expected_prim)
  {
    auto it = env.find(loc);

    if (it == env.end())
      return false;

    auto& info = it->second;

    if (!info.is_default || !info.const_node)
      return false;

    // Use the env type to determine the current primitive, not the AST
    // (the AST is not mutated until post-convergence).
    auto current_prim = extract_primitive(info.type);

    if (!current_prim)
      return false;

    if (current_prim->type() == expected_prim->type())
      return false;

    bool compatible =
      (current_prim->in(integer_types) && expected_prim->in(integer_types)) ||
      (current_prim->in(float_types) && expected_prim->in(float_types));

    if (!compatible)
      return false;

    refine_const(env, info.const_node, expected_prim->type());
    return true;
  }

  // Scope information collected during FuncName navigation.
  struct ScopeInfo
  {
    Node name_elem; // NameElement in the FuncName
    Node def; // ClassDef/Function definition node
  };

  // Navigate a Call node's FuncName from top, collecting scope
  // definitions along the path. Returns the target Function def on
  // success, or an empty Node on failure. Uses find_func_def for
  // Function overload resolution, then rebuilds the scope path.
  Node navigate_call(Node call, Node top, std::vector<ScopeInfo>& scopes)
  {
    assert(call == Call);
    auto funcname = call / FuncName;
    auto args = call / Args;
    auto func_def = find_func_def(top, funcname, args->size(), call / Lhs);

    if (!func_def)
      return {};

    // Collect scope info by re-walking the path.
    Node def = top;

    for (auto it = funcname->begin(); it != funcname->end(); ++it)
    {
      auto& elem = *it;
      auto defs = def->look((elem / Ident)->location());

      bool is_last = (it + 1 == funcname->end());

      if (is_last)
      {
        scopes.push_back({elem, func_def});
      }
      else
      {
        def = defs.front();
        scopes.push_back({elem, def});
      }
    }

    return func_def;
  }

  // Backward-refine a prior Call whose TypeArgs were inferred entirely
  // from default-typed literals. Given an expected type from an outer
  // call context, re-extract TypeParam constraints and update the prior
  // call's TypeArgs, refine its Const literals, and update the env.
  void backward_refine_call(
    Node prior_call, const Node& expected_type, TypeEnv& env, Node top)
  {
    std::vector<ScopeInfo> scopes;
    auto func_def = navigate_call(prior_call, top, scopes);

    if (!func_def)
      return;

    auto args = prior_call / Args;
    auto params = func_def / Params;

    // Get the unsubstituted return type of the prior call's function.
    auto ret_type = func_def / Type;
    auto ret_inner = ret_type->front();
    auto expected_inner = expected_type->front();

    // Extract TypeParam constraints by matching the function's return
    // type (with free TypeParams) against the expected type (concrete).
    NodeMap<LocalTypeInfo> constraints;
    extract_constraints(top, ret_inner, expected_inner, constraints, false);

    // If structural matching didn't find constraints (e.g., return type
    // is a class and expected type is a shape), try reverse shape
    // matching: match class method return types against shape method
    // return types to extract class TypeParam constraints.
    if (
      constraints.empty() && (ret_inner == TypeName) &&
      (expected_inner == TypeName))
    {
      auto ret_def = find_def(top, ret_inner);
      auto exp_def = find_def(top, expected_inner);

      if (
        ret_def && exp_def && (ret_def == ClassDef) &&
        ((ret_def / Shape) != Shape) && (exp_def == ClassDef) &&
        ((exp_def / Shape) == Shape))
      {
        // Build substitution: shape TypeParams -> concrete TypeArgs
        // from the expected type.
        auto shape_to_concrete = build_class_subst(exp_def, expected_inner);

        if (!shape_to_concrete.empty())
        {
          auto shape_body = exp_def / ClassBody;
          auto class_body = ret_def / ClassBody;

          for (auto& shape_func : *shape_body)
          {
            if (shape_func != Function)
              continue;

            auto method_name = (shape_func / Ident)->location().view();
            auto mhand = (shape_func / Lhs)->type();
            auto arity = (shape_func / Params)->size();

            for (auto& class_func : *class_body)
            {
              if (class_func != Function)
                continue;

              if ((class_func / Ident)->location().view() != method_name)
                continue;

              if ((class_func / Lhs)->type() != mhand)
                continue;

              if ((class_func / Params)->size() != arity)
                continue;

              // Class method return has free TypeParams.
              auto class_ret = class_func / Type;

              // Shape method return substituted with concrete values.
              auto concrete_ret =
                apply_subst(top, shape_func / Type, shape_to_concrete);

              extract_constraints(
                top,
                class_ret->front(),
                concrete_ret->front(),
                constraints,
                false);

              break;
            }
          }
        }
      }
    }

    if (constraints.empty())
      return;

    LOG(Trace) << "backward_refine_call: refining TypeArgs for "
               << (prior_call / FuncName)->location().view() << std::endl;

    // Update TypeArgs in the prior call's FuncName.
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

    // Rebuild substitution map from updated TypeArgs.
    NodeMap<Node> subst;

    for (auto& scope : scopes)
    {
      auto ta = scope.name_elem / TypeArgs;
      auto tps = scope.def / TypeParams;

      if (!ta->empty() && ta->size() == tps->size())
      {
        for (size_t i = 0; i < tps->size(); i++)
          subst[tps->at(i)] = ta->at(i);
      }
    }

    // Refine Const literals in the prior call's args.
    for (size_t i = 0; i < params->size(); i++)
    {
      auto param = params->at(i);
      auto arg_node = args->at(i);
      auto formal_type = param / Type;

      Node expected_prim;
      auto tp_def = direct_typeparam(top, formal_type);

      if (tp_def)
      {
        auto find = subst.find(tp_def);

        if (find != subst.end())
          expected_prim = extract_primitive(find->second);
      }
      else
      {
        expected_prim = extract_primitive(formal_type);
      }

      if (!expected_prim)
        continue;

      auto arg_src = arg_node / Rhs;
      try_refine(env, arg_src->location(), expected_prim);
    }

    // Recompute the prior call's result type and update env.
    auto result_type = apply_subst(top, ret_type, subst);

    if (result_type)
    {
      auto dst = prior_call / LocalId;
      env[dst->location()] = LocalTypeInfo::computed(result_type);
    }
  }

  // Backward-refine a prior CallDyn whose result was all-default.
  // Given an expected primitive type from a downstream Call, refine
  // the CallDyn's Const args, re-resolve the Lookup, and update env.
  void backward_refine_calldyn(
    Node calldyn,
    const Node& expected_prim,
    TypeEnv& env,
    Node top,
    std::map<Location, Node>& lookup_stmts)
  {
    assert(calldyn->in({CallDyn, TryCallDyn}));
    assert(expected_prim);
    auto dst = calldyn / LocalId;
    auto src = calldyn / Rhs;
    auto args = calldyn / Args;

    // Find the corresponding Lookup statement.
    auto lookup_it = lookup_stmts.find(src->location());

    if (lookup_it == lookup_stmts.end())
      return;

    auto lookup_node = lookup_it->second;

    // Refine each default-typed Const arg to match expected_prim.
    bool refined = false;

    for (auto& arg_node : *args)
    {
      auto arg_src = arg_node / Rhs;

      if (try_refine(env, arg_src->location(), expected_prim))
        refined = true;
    }

    // Also refine the Lookup receiver if it's a default-typed Const.
    auto lookup_src = lookup_node / Rhs;

    if (try_refine(env, lookup_src->location(), expected_prim))
      refined = true;

    if (!refined)
      return;

    LOG(Trace) << "backward_refine_calldyn: refined to "
               << expected_prim->type().str() << std::endl;

    // Re-resolve the Lookup return type after refinement.
    auto lookup_dst = lookup_node / LocalId;
    auto hand = (lookup_node / Lhs)->type();
    auto method_ident = lookup_node / Ident;
    auto method_ta = lookup_node / TypeArgs;
    auto arity = from_chars_sep_v<size_t>(lookup_node / Int);
    auto recv_it = env.find(lookup_src->location());

    if (recv_it != env.end())
    {
      auto ret_type = resolve_method_return_type(
        top, recv_it->second.type, method_ident, hand, arity, method_ta);

      if (ret_type)
        env[lookup_dst->location()] = LocalTypeInfo::computed(ret_type);
    }

    // Update the CallDyn result type from the updated Lookup.
    auto src_it = env.find(src->location());

    if (src_it != env.end())
    {
      env[dst->location()] =
        LocalTypeInfo::computed(clone(src_it->second.type));
    }
  }

  // Check if a Type node contains TypeVar anywhere (including nested
  // inside ref[TypeVar], cown[TypeVar], etc.).
  static bool contains_typevar(const Node& type_node)
  {
    if (!type_node)
      return false;

    bool found = false;

    type_node->traverse([&](auto node) {
      if (node == TypeVar)
      {
        found = true;
        return false;
      }

      return !found;
    });

    return found;
  }

  // Try to infer TypeArgs and refine Const types at a Call site.
  void infer_call(
    Node call, TypeEnv& env, Node top, std::map<Location, Node>& lookup_stmts)
  {
    std::vector<ScopeInfo> scopes;
    auto func_def = navigate_call(call, top, scopes);

    if (!func_def)
      return;

    auto funcname = call / FuncName;
    auto args = call / Args;
    auto params = func_def / Params;

    // Check if any scope needs TypeArg inference.
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

    // Phase 2: Collect TypeParam constraints from arg types.
    // Non-default types take priority over defaults on conflict.
    // Key: TypeParam def node, Value: {type, is_default}.
    bool all_default_inference = false;
    NodeMap<LocalTypeInfo> constraints;

    if (needs_inference)
    {
      for (size_t i = 0; i < params->size(); i++)
      {
        auto param = params->at(i);
        auto arg_node = args->at(i);
        auto formal_type = param / Type;

        auto arg_src = arg_node / Rhs;
        auto it = env.find(arg_src->location());

        if (it == env.end())
          continue;

        auto& arg_info = it->second;

        // Structurally match formal type against actual type to extract
        // TypeParam constraints. Handles bare T, wrapper[T], unions, etc.
        extract_constraints(
          top,
          formal_type->front(),
          arg_info.type->front(),
          constraints,
          arg_info.is_default);
      }

      // Check if all constraints were from default-typed args.
      all_default_inference = !constraints.empty();

      for (auto& [tp, info] : constraints)
      {
        if (!info.is_default)
        {
          all_default_inference = false;
          break;
        }
      }

      // Fill TypeArgs for scopes that need inference.
      for (auto& scope : scopes)
      {
        auto ta = scope.name_elem / TypeArgs;
        auto tps = scope.def / TypeParams;

        if (!ta->empty() || tps->empty())
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

          // Clone when inserting into TypeArgs to avoid shared nodes.
          new_ta << clone(find->second.type);
        }

        if (all_constrained)
        {
          scope.name_elem->replace(ta, new_ta);
        }
      }
    }

    // Build a substitution map from all TypeArgs (both pre-existing
    // and newly inferred) for Phase 3 literal refinement.
    NodeMap<Node> subst;

    for (auto& scope : scopes)
    {
      auto ta = scope.name_elem / TypeArgs;
      auto tps = scope.def / TypeParams;

      if (!ta->empty() && ta->size() == tps->size())
      {
        for (size_t i = 0; i < tps->size(); i++)
          subst[tps->at(i)] = ta->at(i);
      }
    }

    // Phase 3: Refine Const types based on expected param types.
    for (size_t i = 0; i < params->size(); i++)
    {
      auto param = params->at(i);
      auto arg_node = args->at(i);
      auto formal_type = param / Type;

      // Determine the expected primitive type for this param position.
      Node expected_prim;
      auto tp_def = direct_typeparam(top, formal_type);

      if (tp_def)
      {
        // TypeParam: look up in the substitution map.
        auto find = subst.find(tp_def);

        if (find != subst.end())
          expected_prim = extract_primitive(find->second);
      }
      else
      {
        // Concrete formal param type: extract directly.
        expected_prim = extract_primitive(formal_type);
      }

      if (!expected_prim)
        continue;

      auto arg_src = arg_node / Rhs;
      try_refine(env, arg_src->location(), expected_prim);
    }

    // Infer TypeVar arg types from formal parameter types.
    // When an arg has TypeVar type (e.g., an untyped lambda param),
    // compute the expected type from the callee's formal param type
    // after substitution and update the caller's env.
    for (size_t i = 0; i < params->size(); i++)
    {
      auto param = params->at(i);
      auto arg_node = args->at(i);
      auto formal_type = param / Type;

      auto arg_src = arg_node / Rhs;
      auto it = env.find(arg_src->location());

      if (it == env.end())
        continue;

      auto& arg_info = it->second;

      if (arg_info.type->front() != TypeVar)
        continue;

      auto expected = apply_subst(top, formal_type, subst);

      if (!expected || expected->front() == TypeVar)
        continue;

      assert(expected == Type);
      arg_info.type = expected;
      arg_info.is_fixed = true;
    }

    // Reverse propagation: when a formal param has TypeVar type,
    // resolve it from either a matching FieldDef (if already concrete)
    // or the actual arg type. FieldDef takes priority because it may
    // have been resolved by a sibling method (e.g., apply inferred
    // field types before create is called).
    {
      auto parent_cls = func_def->parent(ClassDef);

      for (size_t i = 0; i < params->size(); i++)
      {
        auto param = params->at(i);
        auto formal_type = param / Type;

        if (!contains_typevar(formal_type))
          continue;

        // Check if a matching FieldDef already has a concrete type.
        Node resolved_type;

        if (parent_cls)
        {
          auto param_name = (param / Ident)->location().view();

          for (auto& child : *(parent_cls / ClassBody))
          {
            if (child != FieldDef)
              continue;

            if ((child / Ident)->location().view() != param_name)
              continue;

            auto ft = child / Type;

            if (!contains_typevar(ft))
              resolved_type = ft;

            break;
          }
        }

        if (!resolved_type)
        {
          // Fall back to the actual arg's type, but only if the arg
          // is not a default-typed literal. Default types (e.g., u64
          // for integer literals) are unreliable and would poison the
          // param type — the correct type will be resolved later when
          // the function body is re-processed in the second pass.
          auto arg_node = args->at(i);
          auto arg_src = arg_node / Rhs;
          auto it = env.find(arg_src->location());

          if (
            it == env.end() || contains_typevar(it->second.type) ||
            it->second.is_default)
            continue;

          resolved_type = it->second.type;
        }

        // Update the callee's param type.
        param->replace(formal_type, clone(resolved_type));

        // Also refine the actual arg if it's a default-typed literal.
        auto resolved_prim = extract_primitive(resolved_type);

        if (resolved_prim)
        {
          auto arg_node = args->at(i);
          auto arg_src = arg_node / Rhs;
          try_refine(env, arg_src->location(), resolved_prim);
        }

        // Update matching FieldDef if still TypeVar.
        if (parent_cls)
        {
          auto param_name = (param / Ident)->location().view();

          for (auto& child : *(parent_cls / ClassBody))
          {
            if (child != FieldDef)
              continue;

            if ((child / Ident)->location().view() != param_name)
              continue;

            auto field_type = child / Type;

            if (contains_typevar(field_type))
              child->replace(field_type, clone(resolved_type));

            break;
          }
        }
      }
    }

    // Shape-to-lambda propagation: when a formal param has shape type
    // and the actual arg is a lambda class, propagate the shape's method
    // signatures to the lambda.
    for (size_t i = 0; i < params->size(); i++)
    {
      auto param = params->at(i);
      auto formal_type = param / Type;
      auto param_type = apply_subst(top, formal_type, subst);

      if (!param_type || param_type->front() == TypeVar)
        continue;

      auto arg_node = args->at(i);
      auto arg_src = arg_node / Rhs;
      auto arg_it = env.find(arg_src->location());

      if (arg_it != env.end())
        propagate_shape_to_lambda(top, param_type, arg_it->second.type);
    }

    // Record the call's result type in the env, with all TypeParam
    // references substituted.
    auto ret_type = func_def / Type;
    auto result_type = apply_subst(top, ret_type, subst);

    if (result_type)
    {
      auto dst = call / LocalId;
      env[dst->location()] = {
        result_type,
        all_default_inference,
        false,
        {},
        all_default_inference ? call : Node{}};
    }

    // Phase 4: Backward refinement of default-inferred call args.
    // When a prior call's TypeArgs were inferred entirely from
    // default-typed literals, and this call provides a non-default
    // expected type, refine the prior call to match.
    for (size_t i = 0; i < params->size(); i++)
    {
      auto param = params->at(i);
      auto arg_node = args->at(i);
      auto formal_type = param / Type;

      auto arg_src = arg_node / Rhs;
      auto arg_it = env.find(arg_src->location());

      if (arg_it == env.end())
        continue;

      auto& arg_info = arg_it->second;

      if (!arg_info.is_default)
        continue;

      if (arg_info.call_node)
      {
        // Backward-refine a prior Call whose TypeArgs were inferred
        // entirely from default-typed literals.
        auto prior_call_node = arg_info.call_node;

        // Compute the expected type for this arg from formal + subst.
        auto expected = apply_subst(top, formal_type, subst);

        if (!expected)
          continue;

        if (prior_call_node == Call)
        {
          backward_refine_call(prior_call_node, expected, env, top);
        }
        else if (prior_call_node->in({CallDyn, TryCallDyn}))
        {
          auto expected_prim = extract_primitive(expected);

          if (expected_prim)
          {
            backward_refine_calldyn(
              prior_call_node, expected_prim, env, top, lookup_stmts);
          }
        }

        // Update the arg's env entry to reflect the refined type.
        auto prior_dst = prior_call_node / LocalId;
        auto updated_it = env.find(prior_dst->location());

        if (updated_it != env.end())
        {
          env[arg_src->location()] =
            LocalTypeInfo::propagated(updated_it->second);
        }
      }
    }
  }

  // Check if a function has any remaining TypeVar param or return types.
  static bool has_typevar(Node func)
  {
    assert(func == Function);

    for (auto& pd : *(func / Params))
    {
      if ((pd / Type)->front() == TypeVar)
        return true;
    }

    if ((func / Type)->front() == TypeVar)
      return true;

    // Also check FieldDefs in the parent class and any nested ClassDefs:
    // if any field contains TypeVar (possibly nested, e.g., ref[TypeVar]),
    // the function body may reference it and needs re-processing. Nested
    // classes capture variables from the enclosing scope via fields, so
    // TypeVar in a nested class's fields means the enclosing function
    // needs to propagate types into the nested class's create call.
    auto parent_cls = func->parent(ClassDef);

    if (parent_cls)
    {
      bool found = false;

      parent_cls->traverse([&](auto node) {
        if (found)
          return false;

        if (node == FieldDef && contains_typevar(node / Type))
        {
          found = true;
          return false;
        }

        return node == parent_cls || node == ClassBody || node == ClassDef;
      });

      if (found)
        return true;
    }

    return false;
  }

  // Merge a type into a union, pruning subtypes. Returns the merged type.
  // If the new type is a subtype of any existing member, returns the
  // existing union unchanged. If any existing member is a subtype of the
  // new type, removes it and adds the new type.
  static Node merge_type(const Node& existing, const Node& incoming, Node top)
  {
    if (!existing || existing->front() == TypeVar)
      return clone(incoming);

    if (!incoming || incoming->front() == TypeVar)
      return clone(existing);

    SequentCtx ctx{top, {}, {}};

    // If incoming is a subtype of existing, keep existing.
    if (Subtype(ctx, incoming, existing))
      return clone(existing);

    // If existing is a subtype of incoming, take incoming.
    if (Subtype(ctx, existing, incoming))
      return clone(incoming);

    // Neither is a subtype — build a union.
    auto e_inner = existing->front();
    auto i_inner = incoming->front();

    Node union_node = Union;

    // Flatten existing into union members.
    if (e_inner == Union)
    {
      for (auto& child : *e_inner)
        union_node << clone(child);
    }
    else
    {
      union_node << clone(e_inner);
    }

    // Add incoming, pruning against existing members.
    bool already_covered = false;

    for (auto& member : *union_node)
    {
      Node wrapped = Type << clone(member);

      if (Subtype(ctx, incoming, wrapped))
      {
        already_covered = true;
        break;
      }
    }

    if (!already_covered)
    {
      // Remove any members that are subtypes of incoming.
      auto it = union_node->begin();

      while (it != union_node->end())
      {
        Node wrapped = Type << clone(*it);

        if (Subtype(ctx, wrapped, incoming))
          it = union_node->erase(it, std::next(it));
        else
          ++it;
      }

      union_node << clone(i_inner);
    }

    if (union_node->size() == 1)
      return Type << clone(union_node->front());

    return Type << union_node;
  }

  // Merge a source TypeEnv into a destination TypeEnv. For each variable,
  // merges types using union with subtype pruning.
  static void merge_env(TypeEnv& dst, const TypeEnv& src, Node top)
  {
    for (auto& [loc, src_info] : src)
    {
      auto it = dst.find(loc);

      if (it == dst.end())
      {
        // First time seeing this location: copy all metadata.
        dst[loc] = {
          clone(src_info.type),
          src_info.is_default,
          src_info.is_fixed,
          src_info.const_node,
          src_info.call_node};
      }
      else
      {
        auto merged = merge_type(it->second.type, src_info.type, top);
        it->second.type = merged;

        // Once merged from multiple paths, the entry is no longer
        // default or fixed, and loses const/call tracking — unless
        // both sources agree on all metadata.
        if (
          it->second.is_default != src_info.is_default ||
          it->second.is_fixed != src_info.is_fixed ||
          it->second.const_node != src_info.const_node ||
          !types_equal(it->second.type, src_info.type))
        {
          it->second.is_default = false;
          it->second.is_fixed = false;
          it->second.const_node = {};
          it->second.call_node = {};
        }
      }
    }
  }

  // Compute a fingerprint string for a type environment, for convergence
  // detection.
  static std::string env_fingerprint(const TypeEnv& env)
  {
    // Use a sorted map for deterministic ordering.
    std::map<std::string, std::string> sorted;

    for (auto& [loc, info] : env)
    {
      auto prim = extract_primitive(info.type);
      std::string type_str;

      if (prim)
        type_str = std::string(prim->type().str());
      else if (info.type->front() == TypeVar)
        type_str = "?";
      else if (info.type->front() == TypeName)
      {
        auto inner = info.type->front();

        for (auto& ne : *inner)
          type_str += std::string((ne / Ident)->location().view()) + "::";
      }
      else if (info.type->front() == Union)
      {
        type_str = "U(";

        for (auto& child : *(info.type->front()))
        {
          Node wrapped = Type << clone(child);
          auto p = extract_primitive(wrapped);

          if (p)
            type_str += std::string(p->type().str()) + "|";
          else if (child == TypeName)
          {
            for (auto& ne : *child)
              type_str += std::string((ne / Ident)->location().view()) + "::";

            type_str += "|";
          }
          else
            type_str += "?|";
        }

        type_str += ")";
      }
      else
        type_str = "dyn";

      sorted[std::string(loc.view())] = type_str;
    }

    std::string fp;

    for (auto& [k, v] : sorted)
      fp += k + ":" + v + ";";

    return fp;
  }

  // Trace backward from a Cond operand through the label body to find a
  // Typetest origin. Returns {typetest_src, typetest_type, negated} or
  // empty nodes if not found.
  struct TypetestTrace
  {
    Node src; // The variable being type-tested
    Node type; // The type being tested against
    bool negated; // True if through an odd number of Not operations
  };

  static std::optional<TypetestTrace>
  trace_typetest(const Node& cond_local, const Node& body)
  {
    auto target_loc = cond_local->location();
    bool negated = false;

    // Scan backward through body statements.
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
        {
          return TypetestTrace{stmt / Rhs, stmt / Type, negated};
        }
      }
    }

    return std::nullopt;
  }

  // Process statements in a label body, updating the type environment.
  // This contains all per-statement type inference logic.
  // Cascade re-propagation: refine all remaining default Consts
  // compatible with `expected_prim`, then propagate through
  // Copy/Move/Lookup/CallDyn until stable. `cascade_changed` tracks
  // affected locations to prevent oscillation.
  static void run_cascade(
    TypeEnv& env,
    Node expected_prim,
    const Node& labels,
    Node top,
    std::map<Location, Node>& lookup_stmts)
  {
    if (!expected_prim)
      return;

    // Refine ALL remaining default Consts of compatible type.
    bool found_default = true;

    while (found_default)
    {
      found_default = false;

      for (auto& [loc, info] : env)
      {
        if (!info.is_default || !info.const_node)
          continue;

        if (try_refine(env, loc, expected_prim))
        {
          found_default = true;
          break;
        }
      }
    }

    // Seed cascade_changed with locations that have the expected
    // primitive type.
    std::set<Location> cascade_changed;

    for (auto& [loc, info] : env)
    {
      auto p = extract_primitive(info.type);

      if (
        p && p->type() == expected_prim->type() && !info.is_default &&
        !info.is_fixed)
      {
        cascade_changed.insert(loc);
      }
    }

    // Cascade: re-iterate all statements in all labels to
    // re-propagate types through Copy/Move, Lookup, and CallDyn.
    bool changed = true;

    while (changed)
    {
      changed = false;

      for (auto& lbl2 : *labels)
      {
        for (auto& stmt2 : *(lbl2 / Body))
        {
          if (stmt2->in({Copy, Move}))
          {
            auto cp_dst = stmt2 / LocalId;
            auto cp_src = stmt2 / Rhs;

            if (
              cascade_changed.find(cp_src->location()) == cascade_changed.end())
              continue;

            auto dst_it = env.find(cp_dst->location());
            auto src_it = env.find(cp_src->location());

            if (
              src_it != env.end() &&
              (dst_it == env.end() || !dst_it->second.is_fixed))
            {
              auto new_info = LocalTypeInfo::propagated(src_it->second);

              if (
                dst_it == env.end() ||
                !types_equal(dst_it->second.type, new_info.type))
              {
                env[cp_dst->location()] = new_info;
                cascade_changed.insert(cp_dst->location());
                changed = true;
              }
            }
          }
          else if (stmt2 == Lookup)
          {
            auto lk_dst = stmt2 / LocalId;
            auto lk_src = stmt2 / Rhs;

            if (
              cascade_changed.find(lk_src->location()) == cascade_changed.end())
              continue;

            auto lk_hand = (stmt2 / Lhs)->type();
            auto lk_ident = stmt2 / Ident;
            auto lk_ta = stmt2 / TypeArgs;
            auto lk_ar = from_chars_sep_v<size_t>(stmt2 / Int);
            auto src_it = env.find(lk_src->location());

            if (src_it != env.end())
            {
              auto rt = resolve_method_return_type(
                top, src_it->second.type, lk_ident, lk_hand, lk_ar, lk_ta);

              if (rt)
              {
                auto dst_it = env.find(lk_dst->location());

                if (
                  dst_it == env.end() || !types_equal(dst_it->second.type, rt))
                {
                  env[lk_dst->location()] = LocalTypeInfo::computed(rt);
                  cascade_changed.insert(lk_dst->location());
                  changed = true;
                }
              }
            }
          }
          else if (stmt2->in({CallDyn, TryCallDyn}))
          {
            auto cd_dst = stmt2 / LocalId;
            auto cd_src = stmt2 / Rhs;
            auto cd_args = stmt2 / Args;

            bool src_changed =
              cascade_changed.find(cd_src->location()) != cascade_changed.end();
            bool arg_changed = false;

            for (auto& arg_node : *cd_args)
            {
              auto as = arg_node / Rhs;

              if (cascade_changed.find(as->location()) != cascade_changed.end())
              {
                arg_changed = true;
                break;
              }
            }

            if (!src_changed && !arg_changed)
              continue;

            auto li = lookup_stmts.find(cd_src->location());

            if (li != lookup_stmts.end())
            {
              auto lk = li->second;
              auto lk_src = lk / Rhs;
              auto ri = env.find(lk_src->location());

              if (ri != env.end())
              {
                auto hi = (lk / Lhs)->type();
                auto mi = lk / Ident;
                auto ta = lk / TypeArgs;
                auto ar = from_chars_sep_v<size_t>(lk / Int);

                auto minfo =
                  resolve_method(top, ri->second.type, mi, hi, ar, ta);

                if (minfo.func)
                {
                  auto mparams = minfo.func / Params;
                  size_t idx = 0;

                  for (auto& arg_node : *cd_args)
                  {
                    if (idx >= mparams->size())
                      break;

                    auto pt =
                      apply_subst(top, mparams->at(idx) / Type, minfo.subst);
                    auto pp = extract_callable_primitive(pt);

                    if (pp)
                    {
                      auto as = arg_node / Rhs;

                      if (try_refine(env, as->location(), pp))
                      {
                        cascade_changed.insert(as->location());
                        changed = true;
                      }
                    }

                    idx++;
                  }
                }
              }
            }

            // Re-propagate CallDyn result type.
            if (src_changed)
            {
              auto src_it = env.find(cd_src->location());

              if (src_it != env.end())
              {
                auto dst_it = env.find(cd_dst->location());

                if (
                  dst_it == env.end() ||
                  !types_equal(dst_it->second.type, src_it->second.type))
                {
                  env[cd_dst->location()] =
                    LocalTypeInfo::computed(clone(src_it->second.type));
                  cascade_changed.insert(cd_dst->location());
                  changed = true;
                }
              }
            }
          }
          else if (stmt2 == RegisterRef)
          {
            // When the source of a RegisterRef is refined, update the
            // ref's type to ref[new_type].
            auto rr_dst = stmt2 / LocalId;
            auto rr_src = stmt2 / Rhs;
            auto rr_src_loc = rr_src->location();

            if (cascade_changed.find(rr_src_loc) == cascade_changed.end())
              continue;

            auto src_it = env.find(rr_src_loc);

            if (src_it != env.end())
            {
              auto new_ref_type = ref_type(clone(src_it->second.type));
              auto dst_it = env.find(rr_dst->location());

              if (
                dst_it == env.end() ||
                !types_equal(dst_it->second.type, new_ref_type))
              {
                env[rr_dst->location()] = LocalTypeInfo::computed(new_ref_type);
                cascade_changed.insert(rr_dst->location());
                changed = true;
              }
            }
          }
          else if (stmt2->in({New, Stack}))
          {
            // When a NewArg's source is refined, update the matching
            // FieldDef in the class.
            auto new_type = stmt2 / Type;
            auto inner = new_type->front();

            if (inner != TypeName)
              continue;

            auto class_def = find_def(top, inner);

            if (!class_def || class_def != ClassDef)
              continue;

            auto class_body = class_def / ClassBody;

            for (auto& new_arg : *(stmt2 / NewArgs))
            {
              auto arg_src = new_arg / Rhs;
              auto arg_loc = arg_src->location();
              bool in_changed =
                cascade_changed.find(arg_loc) != cascade_changed.end();

              if (!in_changed)
                continue;

              auto arg_it = env.find(arg_loc);

              if (arg_it == env.end())
                continue;

              auto field_ident = new_arg / Ident;

              for (auto& child : *class_body)
              {
                if (child != FieldDef)
                  continue;

                if (
                  (child / Ident)->location().view() !=
                  field_ident->location().view())
                  continue;

                auto ft = child / Type;

                if (
                  contains_typevar(ft) || !types_equal(ft, arg_it->second.type))
                {
                  child->replace(ft, clone(arg_it->second.type));
                  changed = true;
                }

                break;
              }
            }
          }
        }
      }
    }
  }

  static void process_label_body(
    const Node& body,
    TypeEnv& env,
    Node top,
    std::map<Location, Node>& lookup_stmts,
    std::vector<std::pair<Location, Location>>& typevar_aliases,
    std::map<Location, std::pair<Location, size_t>>& ref_to_tuple)
  {
    // Track tuple construction from NewArrayConst patterns.
    struct TupleTracking
    {
      Node stmt;
      size_t size;
      std::vector<Node> element_types;
      bool is_array_lit;
      std::vector<Location> element_value_locs;
    };

    // Note: tuple_locals is local to each label processing. That's fine
    // because tuple construction (NewArrayConst, ArrayRefConst, Store)
    // always happens within a single label.
    std::map<Location, TupleTracking> tuple_locals;

    for (auto& stmt : *body)
    {
      if (stmt == Const)
      {
        // Determine default type from literal.
        // Check if already processed (has 3 children: dst, type, lit).
        auto dst = stmt->front();
        Node type;

        if (stmt->size() == 3)
        {
          // Already processed on a prior iteration — read existing type.
          type = stmt->at(1);
        }
        else
        {
          // First encounter: determine type from literal but don't
          // mutate the AST yet.
          auto lit = stmt->back();
          type = default_literal_type(lit);
        }

        // Record in type env.
        bool is_default = type->in({U64, F64});
        env[dst->location()] = is_default ?
          LocalTypeInfo::literal(primitive_type(type->type()), stmt) :
          LocalTypeInfo::computed(primitive_type(type->type()));
      }
      else if (stmt == ConstStr)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(string_type());
      }
      else if (stmt == Convert)
      {
        env[(stmt / LocalId)->location()] =
          LocalTypeInfo::computed(primitive_type((stmt / Type)->type()));
      }
      else if (stmt->in({Copy, Move}))
      {
        auto dst = stmt / LocalId;
        auto src = stmt / Rhs;
        auto dst_it = env.find(dst->location());
        auto src_it = env.find(src->location());

        if (
          dst_it != env.end() && dst_it->second.is_fixed && src_it != env.end())
        {
          // dst has a fixed type (type assertion).
          // If src is a refinable Const, refine to match dst.
          auto dst_prim = extract_primitive(dst_it->second.type);

          if (dst_prim)
            try_refine(env, src->location(), dst_prim);
        }
        else if (
          (dst_it == env.end() || !dst_it->second.is_fixed) &&
          src_it != env.end())
        {
          // Propagate source type.
          env[dst->location()] = LocalTypeInfo::propagated(src_it->second);

          // Track alias for TypeVar back-propagation.
          if (src_it->second.type->front() == TypeVar)
            typevar_aliases.push_back({dst->location(), src->location()});
        }
      }
      else if (stmt == RegisterRef)
      {
        auto dst = stmt / LocalId;
        auto src = stmt / Rhs;
        auto src_it = env.find(src->location());

        if (src_it != env.end())
          env[dst->location()] =
            LocalTypeInfo::computed(ref_type(src_it->second.type));
      }
      else if (stmt == FieldRef)
      {
        auto dst = stmt / LocalId;
        auto arg = stmt / Arg;
        auto field_id = stmt / FieldId;
        auto arg_src = arg / Rhs;
        auto obj_it = env.find(arg_src->location());

        if (obj_it != env.end())
        {
          auto& obj_type = obj_it->second.type;
          auto inner = obj_type->front();

          if (inner == TypeName)
          {
            auto class_def = find_def(top, inner);

            if (class_def && class_def == ClassDef)
            {
              auto field_subst = build_class_subst(class_def, inner);
              auto field_name = field_id->location().view();

              for (auto& child : *(class_def / ClassBody))
              {
                if (child != FieldDef)
                  continue;

                if ((child / Ident)->location().view() != field_name)
                  continue;

                auto ft = apply_subst(top, child / Type, field_subst);
                env[dst->location()] = LocalTypeInfo::computed(ref_type(ft));
                break;
              }
            }
          }
        }
      }
      else if (stmt == Load)
      {
        auto dst = stmt / LocalId;
        auto src = stmt / Rhs;
        auto src_it = env.find(src->location());

        if (src_it != env.end())
        {
          auto inner = extract_ref_inner(src_it->second.type);

          if (inner)
            env[dst->location()] = LocalTypeInfo::computed(inner);
        }
      }
      else if (stmt == Store)
      {
        auto dst = stmt / LocalId;
        auto src = stmt / Rhs;
        auto src_it = env.find(src->location());

        if (src_it != env.end())
        {
          auto inner = extract_ref_inner(src_it->second.type);

          if (inner)
          {
            env[dst->location()] = LocalTypeInfo::computed(inner);

            // Refine the stored value based on ref content type.
            auto expected_prim = extract_primitive(inner);

            if (expected_prim)
            {
              auto arg = stmt / Arg;
              auto val_src = arg / Rhs;
              try_refine(env, val_src->location(), expected_prim);
            }
          }
        }

        // Track tuple element types through Store.
        auto ref_it = ref_to_tuple.find(src->location());

        if (ref_it != ref_to_tuple.end())
        {
          auto& [tup_loc, elem_idx] = ref_it->second;
          auto tup_it = tuple_locals.find(tup_loc);

          if (tup_it != tuple_locals.end() && elem_idx < tup_it->second.size)
          {
            auto arg = stmt / Arg;
            auto val_src = arg / Rhs;
            auto val_it = env.find(val_src->location());

            if (val_it != env.end())
            {
              tup_it->second.element_types[elem_idx] =
                clone(val_it->second.type);

              if (tup_it->second.is_array_lit)
                tup_it->second.element_value_locs[elem_idx] =
                  val_src->location();
            }
          }
        }
      }
      else if (stmt->in({ArrayRef, ArrayRefConst}))
      {
        auto dst = stmt / LocalId;
        auto arr_src = stmt / Arg / Rhs;
        auto arr_it = env.find(arr_src->location());

        if (arr_it != env.end())
        {
          auto type = arr_it->second.type;
          auto inner = type->front();

          if (inner == TupleType && stmt == ArrayRefConst)
          {
            auto idx_node = stmt / Rhs;
            auto idx = from_chars_sep_v<size_t>(idx_node);

            if (idx < inner->size())
            {
              auto elem = Type << clone(inner->at(idx));
              env[dst->location()] = LocalTypeInfo::computed(ref_type(elem));
            }
            else
            {
              auto dyn_type = Type << Dyn;
              env[dst->location()] =
                LocalTypeInfo::computed(ref_type(dyn_type));
            }
          }
          else
          {
            env[dst->location()] =
              LocalTypeInfo::computed(ref_type(arr_it->second.type));

            if (stmt == ArrayRefConst)
            {
              auto tup_it = tuple_locals.find(arr_src->location());

              if (tup_it != tuple_locals.end())
              {
                auto idx_node = stmt / Rhs;
                auto idx = from_chars_sep_v<size_t>(idx_node);
                ref_to_tuple[dst->location()] = {arr_src->location(), idx};
              }
            }
          }
        }
      }
      else if (stmt == ArrayRefFromEnd)
      {
        // Like ArrayRefConst but indexing from the end.
        // Pre-reify, we don't know the concrete index, so type as
        // ref[any].
        auto dst = stmt / LocalId;
        auto any_type = Type
          << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                       << (NameElement << (Ident ^ "any") << TypeArgs));
        env[dst->location()] = LocalTypeInfo::computed(ref_type(any_type));
      }
      else if (stmt == SplatOp)
      {
        // Splat extracts a sub-range of a tuple. Post-reify, the type
        // depends on the remaining element count (none / T / TupleType).
        // Pre-reify, type as any.
        auto dst = stmt / LocalId;
        auto any_type = Type
          << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                       << (NameElement << (Ident ^ "any") << TypeArgs));
        env[dst->location()] = LocalTypeInfo::computed(any_type);
      }
      else if (stmt->in({NewArray, NewArrayConst}))
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(clone(stmt / Type));

        if (stmt == NewArrayConst)
        {
          auto sz_node = stmt / Rhs;
          auto sz = from_chars_sep_v<size_t>(sz_node);
          auto loc_view = dst->location().view();
          bool is_arr =
            loc_view.size() >= 5 && loc_view.substr(0, 5) == "array";
          tuple_locals[dst->location()] = {stmt, sz, {}, is_arr, {}};
          tuple_locals[dst->location()].element_types.resize(sz);

          if (is_arr)
            tuple_locals[dst->location()].element_value_locs.resize(sz);
        }
      }
      else if (stmt == MakePtr)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(ffi_primitive_type(Ptr));
      }
      else if (stmt == TypeAssertion)
      {
        auto local_id = stmt / LocalId;
        env[local_id->location()] = LocalTypeInfo::fixed(clone(stmt / Type));
      }
      else if (stmt == GetRaise)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(U64));
      }
      else if (stmt == SetRaise)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(U64));
      }
      else if (stmt->in({New, Stack}))
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(clone(stmt / Type));

        // Refine Const literals used as New arguments based on
        // field types.
        auto new_type = stmt / Type;
        auto inner = new_type->front();

        if (inner == TypeName)
        {
          auto class_def = find_def(top, inner);

          if (class_def && class_def == ClassDef)
          {
            auto new_subst = build_class_subst(class_def, inner);
            auto class_body = class_def / ClassBody;

            for (auto& new_arg : *(stmt / NewArgs))
            {
              assert(new_arg == NewArg);
              auto field_ident = new_arg / Ident;
              auto arg_src = new_arg / Rhs;
              auto it = env.find(arg_src->location());

              if (it == env.end())
                continue;

              for (auto& child : *class_body)
              {
                if (child != FieldDef)
                  continue;

                if (
                  (child / Ident)->location().view() !=
                  field_ident->location().view())
                  continue;

                auto ft = apply_subst(top, child / Type, new_subst);

                if (
                  it->second.type->front() == TypeVar && ft &&
                  ft->front() != TypeVar)
                {
                  it->second.type = clone(ft);
                  it->second.is_fixed = true;
                }

                auto expected_prim = extract_primitive(ft);

                if (expected_prim)
                  try_refine(env, arg_src->location(), expected_prim);

                // Reverse propagation: when the FieldDef contains
                // TypeVar (possibly nested, e.g., ref[TypeVar]) and
                // the arg has a concrete type, update the FieldDef.
                if (
                  contains_typevar(child / Type) &&
                  !contains_typevar(it->second.type))
                {
                  child->replace(child / Type, clone(it->second.type));
                }

                break;
              }
            }
          }
        }
      }
      else if (stmt->in(
                 {Add,
                  Sub,
                  Mul,
                  Div,
                  Mod,
                  Pow,
                  And,
                  Or,
                  Xor,
                  Shl,
                  Shr,
                  Min,
                  Max,
                  LogBase,
                  Atan2}))
      {
        auto dst = stmt / LocalId;
        auto lhs = stmt / Lhs;
        auto it = env.find(lhs->location());

        if (it != env.end())
          env[dst->location()] =
            LocalTypeInfo::computed(clone(it->second.type));
      }
      else if (stmt->in({Eq, Ne, Lt, Le, Gt, Ge}))
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(Bool));
      }
      else if (stmt->in({Neg,  Abs,  Ceil, Floor, Exp,   Log,   Sqrt,
                         Cbrt, Sin,  Cos,  Tan,   Asin,  Acos,  Atan,
                         Sinh, Cosh, Tanh, Asinh, Acosh, Atanh, Read}))
      {
        auto dst = stmt / LocalId;
        auto src = stmt / Rhs;
        auto it = env.find(src->location());

        if (it != env.end())
          env[dst->location()] =
            LocalTypeInfo::computed(clone(it->second.type));
      }
      else if (stmt->in({IsInf, IsNaN, Not}))
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(Bool));
      }
      else if (stmt == Bits)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(U64));
      }
      else if (stmt == Len)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(USize));
      }
      else if (stmt->in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(F64));
      }
      else if (stmt == MakeCallback)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] =
          LocalTypeInfo::computed(ffi_primitive_type(Callback));
      }
      else if (stmt == CallbackPtr)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(ffi_primitive_type(Ptr));
      }
      else if (stmt == FreeCallback)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(None));
      }
      else if (stmt->in({AddExternal, RemoveExternal}))
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(None));
      }
      else if (stmt == RegisterExternalNotify)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(None));
      }
      else if (stmt == Call)
      {
        infer_call(stmt, env, top, lookup_stmts);
      }
      else if (stmt == Lookup)
      {
        auto dst = stmt / LocalId;
        auto src = stmt / Rhs;
        auto hand = (stmt / Lhs)->type();
        auto method_ident = stmt / Ident;
        auto method_ta = stmt / TypeArgs;
        auto arity = from_chars_sep_v<size_t>(stmt / Int);
        auto src_it = env.find(src->location());

        if (src_it != env.end())
        {
          auto ret_type = resolve_method_return_type(
            top, src_it->second.type, method_ident, hand, arity, method_ta);

          if (ret_type)
            env[dst->location()] = LocalTypeInfo::computed(ret_type);
        }

        lookup_stmts[dst->location()] = stmt;
      }
      else if (stmt->in({CallDyn, TryCallDyn}))
      {
        auto dst = stmt / LocalId;
        auto src = stmt / Rhs;
        auto args = stmt / Args;
        bool refined = false;

        auto lookup_it = lookup_stmts.find(src->location());

        // Phase 1: Parameter-type-based refinement.
        if (lookup_it != lookup_stmts.end())
        {
          auto lookup_node = lookup_it->second;
          auto lookup_src = lookup_node / Rhs;
          auto recv_it = env.find(lookup_src->location());

          if (recv_it != env.end())
          {
            auto hand = (lookup_node / Lhs)->type();
            auto method_ident = lookup_node / Ident;
            auto method_ta = lookup_node / TypeArgs;
            auto arity = from_chars_sep_v<size_t>(lookup_node / Int);

            auto info = resolve_method(
              top, recv_it->second.type, method_ident, hand, arity, method_ta);

            if (info.func)
            {
              auto params = info.func / Params;
              bool recv_confirmed = !recv_it->second.is_default;
              size_t i = 0;

              for (auto& arg_node : *args)
              {
                if (i >= params->size())
                  break;

                auto param_type =
                  apply_subst(top, params->at(i) / Type, info.subst);
                auto prim = extract_callable_primitive(param_type);

                if (prim)
                {
                  auto arg_src = arg_node / Rhs;

                  if (try_refine(env, arg_src->location(), prim))
                    refined = true;
                  else if (recv_confirmed)
                  {
                    auto arg_it = env.find(arg_src->location());

                    if (arg_it != env.end() && arg_it->second.is_default)
                    {
                      auto arg_prim = extract_primitive(arg_it->second.type);

                      if (arg_prim && arg_prim->type() == prim->type())
                      {
                        arg_it->second.is_default = false;
                        arg_it->second.const_node = {};
                      }
                    }
                  }
                }

                if (param_type && param_type->front() != TypeVar)
                {
                  auto arg_src = arg_node / Rhs;
                  auto arg_it = env.find(arg_src->location());

                  if (
                    arg_it != env.end() &&
                    arg_it->second.type->front() == TypeVar)
                  {
                    arg_it->second.type = clone(param_type);
                    arg_it->second.is_fixed = true;
                  }
                }

                i++;
              }

              // Reverse propagation: TypeVar params from FieldDef or
              // actual args.
              auto parent_cls = info.func->parent(ClassDef);
              i = 0;

              for (auto& arg_node : *args)
              {
                if (i >= params->size())
                  break;

                auto param = params->at(i);
                auto formal_type = param / Type;

                if (contains_typevar(formal_type))
                {
                  Node resolved_type;

                  if (parent_cls)
                  {
                    auto pname = (param / Ident)->location().view();

                    for (auto& child : *(parent_cls / ClassBody))
                    {
                      if (child != FieldDef)
                        continue;

                      if ((child / Ident)->location().view() != pname)
                        continue;

                      auto ft = child / Type;

                      if (!contains_typevar(ft))
                        resolved_type = ft;

                      break;
                    }
                  }

                  if (!resolved_type)
                  {
                    auto arg_src = arg_node / Rhs;
                    auto arg_it = env.find(arg_src->location());

                    if (
                      arg_it != env.end() &&
                      !contains_typevar(arg_it->second.type) &&
                      !arg_it->second.is_default)
                    {
                      resolved_type = arg_it->second.type;
                    }
                  }

                  if (resolved_type)
                  {
                    param->replace(formal_type, clone(resolved_type));

                    auto resolved_prim = extract_primitive(resolved_type);

                    if (resolved_prim)
                    {
                      auto arg_src = arg_node / Rhs;
                      try_refine(env, arg_src->location(), resolved_prim);
                    }

                    if (parent_cls)
                    {
                      auto pname = (param / Ident)->location().view();

                      for (auto& child : *(parent_cls / ClassBody))
                      {
                        if (child != FieldDef)
                          continue;

                        if ((child / Ident)->location().view() != pname)
                          continue;

                        auto ft = child / Type;

                        if (contains_typevar(ft))
                          child->replace(ft, clone(resolved_type));

                        break;
                      }
                    }
                  }
                }

                i++;
              }

              // Shape-to-lambda propagation: when a formal param
              // has shape type and the actual arg is a lambda class,
              // propagate the shape's method signatures to the lambda.
              i = 0;

              for (auto& arg_node : *args)
              {
                if (i >= params->size())
                  break;

                auto param_type =
                  apply_subst(top, params->at(i) / Type, info.subst);

                if (param_type && param_type->front() != TypeVar)
                {
                  auto arg_src = arg_node / Rhs;
                  auto arg_it = env.find(arg_src->location());

                  if (arg_it != env.end())
                    propagate_shape_to_lambda(
                      top, param_type, arg_it->second.type);
                }

                i++;
              }
            }
          }
        }

        // Phase 2: Fallback arg-to-arg refinement.
        if (!refined)
        {
          Node target_prim;

          for (auto& arg_node : *args)
          {
            auto arg_src = arg_node / Rhs;
            auto it = env.find(arg_src->location());

            if (it == env.end() || it->second.is_default)
              continue;

            auto prim = extract_callable_primitive(it->second.type);

            if (!prim)
              prim = extract_wrapper_primitive(it->second.type);

            if (prim)
            {
              target_prim = prim;
              break;
            }
          }

          if (!target_prim && lookup_it != lookup_stmts.end())
          {
            auto lookup_node = lookup_it->second;
            auto lookup_src = lookup_node / Rhs;
            auto recv_it = env.find(lookup_src->location());

            if (recv_it != env.end() && !recv_it->second.is_default)
            {
              auto prim = extract_callable_primitive(recv_it->second.type);

              if (!prim)
                prim = extract_wrapper_primitive(recv_it->second.type);

              if (prim)
                target_prim = prim;
            }
          }

          if (target_prim)
          {
            for (auto& arg_node : *args)
            {
              auto arg_src = arg_node / Rhs;

              if (try_refine(env, arg_src->location(), target_prim))
                refined = true;
            }
          }
        }

        // Re-resolve the Lookup return type after refinement.
        if (refined && lookup_it != lookup_stmts.end())
        {
          auto lookup_node = lookup_it->second;
          auto lookup_src = lookup_node / Rhs;
          auto lookup_dst = lookup_node / LocalId;
          auto hand = (lookup_node / Lhs)->type();
          auto method_ident = lookup_node / Ident;
          auto method_ta = lookup_node / TypeArgs;
          auto arity = from_chars_sep_v<size_t>(lookup_node / Int);
          auto recv_it = env.find(lookup_src->location());

          if (recv_it != env.end())
          {
            auto ret_type = resolve_method_return_type(
              top, recv_it->second.type, method_ident, hand, arity, method_ta);

            if (ret_type)
              env[lookup_dst->location()] = LocalTypeInfo::computed(ret_type);
          }
        }

        // Propagate the Lookup result type to CallDyn result.
        auto src_it = env.find(src->location());

        if (src_it != env.end())
        {
          auto info = LocalTypeInfo::computed(clone(src_it->second.type));

          if (lookup_it != lookup_stmts.end())
          {
            auto recv_loc = (lookup_it->second / Rhs)->location();
            auto recv_it = env.find(recv_loc);

            if (recv_it != env.end() && recv_it->second.is_default)
            {
              info.is_default = true;
              info.call_node = stmt;
            }
          }

          env[dst->location()] = info;
        }
      }
      else if (stmt == FFI)
      {
        auto dst = stmt / LocalId;
        auto sym_id = stmt / SymbolId;
        auto sym_name = sym_id->location();
        auto cls = body->parent(Function)->parent(ClassDef);
        bool found = false;

        while (cls && !found)
        {
          for (auto& child : *(cls / ClassBody))
          {
            if (child != Lib)
              continue;

            for (auto& sym : *(child / Symbols))
            {
              if (sym != Symbol)
                continue;

              if ((sym / SymbolId)->location() == sym_name)
              {
                auto ret_type = sym / Type;

                if (!ret_type->empty())
                  env[dst->location()] =
                    LocalTypeInfo::computed(clone(ret_type));

                // Refine FFI arguments against declared parameter types.
                auto ffi_params = sym / FFIParams;
                auto args = stmt / Args;
                auto fp_it = ffi_params->begin();
                auto ar_it = args->begin();

                while (fp_it != ffi_params->end() && ar_it != args->end())
                {
                  auto expected_prim = extract_primitive(*fp_it);

                  if (expected_prim)
                    try_refine(env, (*ar_it)->location(), expected_prim);

                  ++fp_it;
                  ++ar_it;
                }

                found = true;
                break;
              }
            }

            if (found)
              break;
          }

          cls = cls->parent(ClassDef);
        }
      }
      else if (stmt == When)
      {
        auto dst = stmt / LocalId;
        auto src = stmt / Rhs;
        auto src_it = env.find(src->location());

        if (src_it != env.end())
        {
          auto apply_ret = src_it->second.type;

          auto old_type = stmt / Type;
          stmt->replace(old_type, clone(apply_ret));

          env[dst->location()] = LocalTypeInfo::computed(cown_type(apply_ret));
        }

        auto lookup_it = lookup_stmts.find(src->location());

        if (lookup_it != lookup_stmts.end())
        {
          auto lookup_node = lookup_it->second;
          auto recv_local = lookup_node / Rhs;
          auto recv_it = env.find(recv_local->location());

          if (recv_it != env.end())
          {
            auto recv_type = recv_it->second.type;
            auto recv_inner = recv_type->front();

            if (recv_inner == TypeName)
            {
              auto class_def = find_def(top, recv_inner);

              if (class_def && class_def == ClassDef)
              {
                Node apply_func;

                for (auto& child : *(class_def / ClassBody))
                {
                  if (
                    child == Function &&
                    (child / Ident)->location().view() == "apply")
                  {
                    apply_func = child;
                    break;
                  }
                }

                if (apply_func)
                {
                  auto params = apply_func / Params;
                  auto args = stmt / Args;

                  for (size_t i = 1; i < args->size() && i < params->size();
                       ++i)
                  {
                    auto param = params->at(i);
                    auto param_type = param / Type;

                    if (param_type->front() != TypeVar)
                      continue;

                    auto arg_local = args->at(i) / Rhs;
                    auto arg_it = env.find(arg_local->location());

                    if (arg_it == env.end())
                      continue;

                    auto cown_inner = extract_cown_inner(arg_it->second.type);

                    if (!cown_inner)
                      continue;

                    auto new_type = ref_type(cown_inner);
                    param->replace(param_type, new_type);
                  }
                }
              }
            }
          }
        }
      }
      else if (stmt == Typetest)
      {
        auto dst = stmt / LocalId;
        env[dst->location()] = LocalTypeInfo::computed(primitive_type(Bool));
      }
    }

    // Finalize tuple types and array literal types within this label.
    for (auto& [loc, tracking] : tuple_locals)
    {
      if (tracking.is_array_lit)
      {
        Node dominant_prim;

        for (size_t i = 0; i < tracking.size; i++)
        {
          if (i >= tracking.element_value_locs.size())
            continue;

          auto& elem_loc = tracking.element_value_locs[i];

          if (elem_loc.view().empty())
            continue;

          auto it = env.find(elem_loc);

          if (it == env.end())
            continue;

          if (!it->second.is_default)
          {
            dominant_prim = extract_primitive(it->second.type);

            if (dominant_prim)
              break;
          }
        }

        if (dominant_prim)
        {
          for (size_t i = 0; i < tracking.size; i++)
          {
            if (i >= tracking.element_value_locs.size())
              continue;

            auto& elem_loc = tracking.element_value_locs[i];

            if (!elem_loc.view().empty())
              try_refine(env, elem_loc, dominant_prim);
          }
        }

        Node common_type;
        bool all_same = true;

        for (size_t i = 0; i < tracking.size; i++)
        {
          if (i >= tracking.element_value_locs.size())
          {
            all_same = false;
            break;
          }

          auto& elem_loc = tracking.element_value_locs[i];

          if (elem_loc.view().empty())
          {
            all_same = false;
            break;
          }

          auto it = env.find(elem_loc);

          if (it == env.end())
          {
            all_same = false;
            break;
          }

          auto elem_prim = extract_primitive(it->second.type);

          if (!elem_prim)
          {
            all_same = false;
            break;
          }

          if (!common_type)
            common_type = clone(it->second.type);
          else if (
            it->second.type->front()->type() != common_type->front()->type())
          {
            all_same = false;
            break;
          }
        }

        if (all_same && common_type && tracking.size > 0)
        {
          env[loc] = LocalTypeInfo::computed(clone(common_type));
        }
      }
      else
      {
        bool all_typed = true;

        for (size_t i = 0; i < tracking.size; i++)
        {
          if (!tracking.element_types[i])
          {
            all_typed = false;
            break;
          }
        }

        if (all_typed && tracking.size > 0)
        {
          if (tracking.size == 1)
          {
            auto& elem = tracking.element_types[0];
            env[loc] = LocalTypeInfo::computed(Type << clone(elem->front()));
          }
          else
          {
            Node tuple_type = TupleType;

            for (size_t i = 0; i < tracking.size; i++)
            {
              auto& elem = tracking.element_types[i];
              tuple_type << clone(elem->front());
            }

            env[loc] = LocalTypeInfo::computed(Type << clone(tuple_type));
          }
        }
      }
    }
  }

  // Process a single function for type inference.
  // Returns true if the function was processed without error.
  static bool process_function(Node node, Node top, bool check_errors)
  {
    auto labels = node / Labels;
    size_t n_labels = labels->size();

    if (n_labels == 0)
      return true;

    // ---------- Label indexing and pred/succ graph ----------

    // Map label name → index.
    std::map<std::string, size_t> label_index;

    for (size_t i = 0; i < n_labels; i++)
    {
      auto name = std::string((labels->at(i) / LabelId)->location().view());
      label_index[name] = i;
    }

    // Build successor lists from terminators.
    std::vector<std::vector<size_t>> succ(n_labels);
    std::vector<std::vector<size_t>> pred(n_labels);

    for (size_t i = 0; i < n_labels; i++)
    {
      auto term = labels->at(i) / Return;

      if (term == Cond)
      {
        auto true_name = std::string((term / Lhs)->location().view());
        auto false_name = std::string((term / Rhs)->location().view());
        auto t_it = label_index.find(true_name);
        auto f_it = label_index.find(false_name);

        if (t_it != label_index.end())
          succ[i].push_back(t_it->second);

        if (f_it != label_index.end())
          succ[i].push_back(f_it->second);
      }
      else if (term == Jump)
      {
        auto target = std::string((term / LabelId)->location().view());
        auto it = label_index.find(target);

        if (it != label_index.end())
          succ[i].push_back(it->second);
      }
      // Return and Raise have no successors.
    }

    // Build predecessor lists from successors.
    for (size_t i = 0; i < n_labels; i++)
    {
      for (auto s : succ[i])
        pred[s].push_back(i);
    }

    // ---------- Phase A: Forward pass (type inference + refinement) ----------

    // Single accumulated env, process all labels in order. This handles
    // type inference, call-arg refinement, return-value refinement, etc.
    // Same as the old single-pass approach.
    TypeEnv env;
    auto params = node / Params;

    for (auto& pd : *params)
    {
      assert(pd == ParamDef);
      auto ident = pd / Ident;
      auto type = pd / Type;
      bool is_fixed = type->front() != TypeVar;
      env[ident->location()] = is_fixed ? LocalTypeInfo::fixed(clone(type)) :
                                          LocalTypeInfo::computed(clone(type));
    }

    // Shared state across labels.
    std::map<Location, Node> lookup_stmts;
    std::vector<std::pair<Location, Location>> typevar_aliases;
    std::map<Location, std::pair<Location, size_t>> ref_to_tuple;

    for (auto& lbl : *labels)
    {
      process_label_body(
        lbl / Body, env, top, lookup_stmts, typevar_aliases, ref_to_tuple);

      // Refine return values against the function's declared return type.
      auto term = lbl / Return;

      if (term == Return)
      {
        auto func_ret = node / Type;
        auto ret_prim = extract_primitive(func_ret);

        if (ret_prim)
        {
          auto ret_src = term / LocalId;
          try_refine(env, ret_src->location(), ret_prim);
        }
      }
      else if (term == Raise)
      {
        // Raise carries the enclosing function's return type as its Type
        // child. Refine the raised value's literal against this type.
        auto raise_ret = term / Type;
        auto ret_prim = extract_primitive(raise_ret);

        if (ret_prim)
        {
          auto ret_src = term / LocalId;
          try_refine(env, ret_src->location(), ret_prim);
        }
      }
    }

    // ---------- Phase A.5: Pre-Phase B cascade ----------
    // Run the cascade BEFORE Phase B so that init_env has correctly
    // refined types. Without this, Phase B uses default u64/f64 types,
    // causing wrong method resolution and incorrect arg refinement.
    {
      auto func_ret = node / Type;
      auto expected_prim = extract_primitive(func_ret);
      run_cascade(env, expected_prim, labels, top, lookup_stmts);
    }

    // ---------- Phase B: Convergence loop (typetest narrowing) ----------

    // Per-label environments for tracking narrowed types at Cond/Typetest.
    // Only needed if there are Cond terminators with Typetest traces.
    std::vector<TypeEnv> exit_envs(n_labels);
    std::vector<std::string> fingerprints(n_labels);
    std::vector<std::map<std::string, TypeEnv>> branch_exits(n_labels);

    // Initialize exit envs: copy the Phase A env for all labels.
    // Per-label narrowing will refine from this baseline.
    TypeEnv init_env = env;

    // Check if we actually need the convergence loop.
    bool has_typetest_conds = false;

    for (size_t i = 0; i < n_labels; i++)
    {
      auto term = labels->at(i) / Return;

      if (term == Cond)
      {
        auto trace = trace_typetest(term / LocalId, labels->at(i) / Body);

        if (trace)
        {
          has_typetest_conds = true;
          break;
        }
      }
    }

    if (has_typetest_conds)
    {
      std::queue<size_t> worklist;
      std::vector<bool> in_worklist(n_labels, true);

      for (size_t i = 0; i < n_labels; i++)
        worklist.push(i);

      bool made_progress = true;

      while (!worklist.empty())
      {
        if (!made_progress)
        {
          assert(false && "infer: worklist not converging");
          break;
        }

        made_progress = false;
        size_t wl_size = worklist.size();

        for (size_t wl_iter = 0; wl_iter < wl_size; wl_iter++)
        {
          auto idx = worklist.front();
          worklist.pop();
          in_worklist[idx] = false;

          auto lbl = labels->at(idx);

          // Build entry env: start from Phase A baseline, then apply
          // branch-specific narrowing from predecessors. Branch exits
          // represent intersection constraints (use-site demands from
          // typetests), so narrowed entries OVERRIDE the baseline
          // rather than being unioned with it.
          TypeEnv label_env = init_env;

          for (auto p_idx : pred[idx])
          {
            auto label_name = std::string((lbl / LabelId)->location().view());
            auto& be = branch_exits[p_idx];
            auto be_it = be.find(label_name);

            if (be_it != be.end())
            {
              // Apply branch-specific narrowing: overwrite entries
              // that the branch exit narrowed (typetest constraints).
              for (auto& [loc, binfo] : be_it->second)
              {
                auto it = label_env.find(loc);

                if (it == label_env.end())
                {
                  label_env[loc] = {
                    clone(binfo.type),
                    binfo.is_default,
                    binfo.is_fixed,
                    binfo.const_node,
                    binfo.call_node};
                }
                else
                {
                  // Overwrite with narrowed type. This is correct
                  // because the branch exit represents the type
                  // AFTER the typetest passed — it's a demand, not
                  // a possible assignment.
                  it->second = {
                    clone(binfo.type),
                    binfo.is_default,
                    binfo.is_fixed,
                    binfo.const_node,
                    binfo.call_node};
                }
              }
            }
            else if (!exit_envs[p_idx].empty())
              merge_env(label_env, exit_envs[p_idx], top);
          }

          // Re-process label body with narrowed types.
          process_label_body(
            lbl / Body,
            label_env,
            top,
            lookup_stmts,
            typevar_aliases,
            ref_to_tuple);

          // Handle Typetest narrowing at Cond terminators.
          branch_exits[idx].clear();
          auto term = lbl / Return;

          if (term == Cond)
          {
            auto cond_local = term / LocalId;
            auto trace = trace_typetest(cond_local, lbl / Body);

            if (trace)
            {
              auto src_loc = trace->src->location();
              auto true_name = std::string((term / Lhs)->location().view());
              auto false_name = std::string((term / Rhs)->location().view());

              auto narrowed_type = clone(trace->type);

              if (!trace->negated)
              {
                TypeEnv true_env = label_env;
                true_env[src_loc] =
                  LocalTypeInfo::computed(clone(narrowed_type));
                branch_exits[idx][true_name] = std::move(true_env);
                branch_exits[idx][false_name] = label_env;
              }
              else
              {
                TypeEnv false_env = label_env;
                false_env[src_loc] =
                  LocalTypeInfo::computed(clone(narrowed_type));
                branch_exits[idx][false_name] = std::move(false_env);
                branch_exits[idx][true_name] = label_env;
              }
            }
          }

          // Check convergence.
          auto fp = env_fingerprint(label_env);

          if (fp != fingerprints[idx])
          {
            fingerprints[idx] = fp;
            exit_envs[idx] = std::move(label_env);
            made_progress = true;

            for (auto s : succ[idx])
            {
              if (!in_worklist[s])
              {
                in_worklist[s] = true;
                worklist.push(s);
              }
            }
          }
        }
      }

      // No merge-back: Phase B exit envs are used directly for
      // per-label Const finalization. Merging would union Phase B's
      // fresh Const defaults (u64) with the cascaded env (i32),
      // producing incorrect Union(i32, u64) types.
    }

    // ---------- Cascade re-propagation ----------
    // After all type inference (including Phase B narrowing), propagate
    // refined types through ALL remaining default Consts of compatible
    // type, then cascade through Copy/Move/Lookup/CallDyn until stable.
    {
      auto func_ret = node / Type;
      auto expected_prim = extract_primitive(func_ret);
      run_cascade(env, expected_prim, labels, top, lookup_stmts);
    }

    // ---------- Post-convergence: finalize Const AST nodes ----------

    // Now that types are converged, write final types to Const nodes.
    // For each Const, check both the per-label exit env (which captures
    // use-site demands like CallDyn arg refinement after typetest
    // narrowing) and the global env (which captures cascade refinement
    // from return type). Prefer the one that's non-default (refined).
    for (size_t lbl_idx = 0; lbl_idx < n_labels; lbl_idx++)
    {
      auto& lbl = labels->at(lbl_idx);
      bool have_label_env = has_typetest_conds && !exit_envs[lbl_idx].empty();

      for (auto& stmt : *(lbl / Body))
      {
        if (stmt != Const)
          continue;

        // Find the best type for this Const: check per-label env
        // first (captures use-site demands), then global env
        // (captures cascade). Prefer the non-default refined entry.
        auto dst = (stmt->size() == 3) ? stmt->front() : stmt->front();
        Node final_type;
        auto loc = dst->location();

        // Check per-label env.
        if (have_label_env)
        {
          auto it = exit_envs[lbl_idx].find(loc);

          if (it != exit_envs[lbl_idx].end() && !it->second.is_default)
          {
            auto p = extract_primitive(it->second.type);

            if (p)
              final_type = p;
          }
        }

        // If per-label didn't refine, check global env.
        if (!final_type)
        {
          auto it = env.find(loc);

          if (it != env.end())
          {
            auto p = extract_primitive(it->second.type);

            if (p)
              final_type = p;
          }
        }

        if (stmt->size() == 3)
        {
          if (final_type)
          {
            auto old_type = stmt->at(1);

            if (old_type->type() != final_type->type())
              stmt->replace(old_type, final_type->type());
          }
        }
        else if (stmt->size() == 2)
        {
          auto lit = stmt->back();
          Node type_token;

          if (final_type)
            type_token = final_type->type();
          else
            type_token = default_literal_type(lit);

          stmt->erase(stmt->begin(), stmt->end());
          stmt << dst << type_token << lit;
        }
      }
    }

    // ---------- Post-convergence: finalize tuple/array types ----------

    // Re-process labels once more to finalize NewArrayConst types
    // using the converged env.
    for (auto& lbl : *labels)
    {
      for (auto& stmt : *(lbl / Body))
      {
        if (stmt != NewArrayConst)
          continue;

        auto dst = stmt / LocalId;
        auto it = env.find(dst->location());

        if (it == env.end())
          continue;

        auto inner = it->second.type->front();

        if (inner == TupleType)
        {
          auto old_type = stmt / Type;
          stmt->replace(old_type, clone(it->second.type));
        }
        else
        {
          // Array literal or homogeneous tuple.
          auto prim = extract_primitive(it->second.type);

          if (prim)
          {
            auto old_type = stmt / Type;

            if (!types_equal(old_type, it->second.type))
              stmt->replace(old_type, clone(it->second.type));
          }
        }
      }
    }

    // ---------- Post-convergence: TypeVar back-propagation ----------

    {
      bool changed = true;

      while (changed)
      {
        changed = false;

        for (auto& [dst_loc, src_loc] : typevar_aliases)
        {
          auto dst_it = env.find(dst_loc);
          auto src_it = env.find(src_loc);

          if (dst_it == env.end() || src_it == env.end())
            continue;

          bool dst_tv = dst_it->second.type->front() == TypeVar;
          bool src_tv = src_it->second.type->front() == TypeVar;

          if (!dst_tv && src_tv)
          {
            src_it->second.type = clone(dst_it->second.type);
            src_it->second.is_fixed = true;
            changed = true;
          }
          else if (dst_tv && !src_tv)
          {
            dst_it->second.type = clone(src_it->second.type);
            changed = true;
          }
        }
      }
    }

    // ---------- Post-convergence: update params and fields ----------

    auto parent_cls = node->parent(ClassDef);

    for (auto& pd : *params)
    {
      auto type = pd / Type;

      if (type->front() != TypeVar)
        continue;

      auto ident = pd / Ident;
      auto it = env.find(ident->location());

      if (it == env.end())
        continue;

      if (it->second.type->front() == TypeVar)
        continue;

      pd->replace(type, clone(it->second.type));

      if (parent_cls)
      {
        auto pname = ident->location().view();

        for (auto& child : *(parent_cls / ClassBody))
        {
          if (child != FieldDef)
            continue;

          if ((child / Ident)->location().view() != pname)
            continue;

          auto ft = child / Type;

          if (contains_typevar(ft))
            child->replace(ft, clone(it->second.type));

          break;
        }
      }
    }

    if (parent_cls)
    {
      for (auto& child : *(parent_cls / ClassBody))
      {
        if (child != FieldDef)
          continue;

        auto ft = child / Type;

        if (ft->front() != TypeVar)
          continue;

        auto fname = (child / Ident)->location();
        auto it = env.find(fname);

        if (it == env.end())
          continue;

        if (it->second.type->front() == TypeVar)
          continue;

        child->replace(ft, clone(it->second.type));
      }
    }

    // ---------- Post-convergence: return type inference ----------

    bool in_generic_context = (node / TypeParams)->size() > 0 ||
      ((node->parent({ClassDef}) != nullptr) &&
       (node->parent({ClassDef}) / TypeParams)->size() > 0);

    auto func_ret_type = node / Type;

    if (func_ret_type->front() == TypeVar)
    {
      SequentCtx ctx{top, {}, {}};
      Nodes ret_types;
      bool has_unresolved_return = false;

      for (size_t i = 0; i < n_labels; i++)
      {
        auto term = labels->at(i) / Return;

        if (term != Return)
          continue;

        auto ret_src = term / LocalId;

        // Prefer per-label exit env (has typetest narrowing from Phase B)
        // over the global env. This matches how Const finalization works:
        // exit_envs capture the narrowed type at the return point (e.g.,
        // after a typetest narrows x: any → x: i32), while the global
        // env only has the un-narrowed type.
        Node ret_type_node;

        if (has_typetest_conds && !exit_envs[i].empty())
        {
          auto eit = exit_envs[i].find(ret_src->location());

          if (eit != exit_envs[i].end() && eit->second.type->front() != TypeVar)
            ret_type_node = eit->second.type;
        }

        if (!ret_type_node)
        {
          auto it = env.find(ret_src->location());

          if (it != env.end() && it->second.type->front() != TypeVar)
            ret_type_node = it->second.type;
        }

        if (!ret_type_node)
        {
          // In a generic context, unresolved return types mean the
          // method calls on type parameters couldn't be resolved.
          // Leave the return type as TypeVar for the reify pass.
          if (in_generic_context)
            has_unresolved_return = true;

          continue;
        }

        bool already_covered = false;

        for (auto& existing : ret_types)
        {
          if (Subtype(ctx, ret_type_node, existing))
          {
            already_covered = true;
            break;
          }
        }

        if (!already_covered)
          ret_types.push_back(clone(ret_type_node));
      }

      // In a generic context, if any return path has an unresolved type
      // (e.g., a method call on a type parameter), leave the return type
      // as TypeVar. The reify pass will determine the concrete type after
      // monomorphization.
      if (has_unresolved_return)
      {
        // Don't set the return type — leave as TypeVar.
      }
      else if (ret_types.size() == 1)
      {
        node->replace(func_ret_type, ret_types.front());
      }
      else if (ret_types.size() > 1)
      {
        Node union_node = Union;

        for (auto& rt : ret_types)
          union_node << clone(rt->front());

        node->replace(func_ret_type, Type << union_node);
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
        {
          node->replace(
            func_ret_type,
            Type
              << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                           << (NameElement << (Ident ^ "none") << TypeArgs)));
        }
      }
    }

    // ---------- Post-convergence: error checking ----------

    if (check_errors && !in_generic_context)
    {
      for (auto& pd : *params)
      {
        auto type = pd / Type;

        if (type->front() == TypeVar)
        {
          node->parent()->replace(
            node, err(pd / Ident, "Cannot infer type of parameter"));
          return false;
        }
      }

      func_ret_type = node / Type;

      if (func_ret_type->front() == TypeVar)
      {
        node->parent()->replace(
          node, err(node / Ident, "Cannot infer return type of function"));
        return false;
      }
    }

    // ---------- Post-convergence: cleanup ----------

    for (auto& lbl : *labels)
    {
      auto body = lbl / Body;
      auto it = body->begin();

      while (it != body->end())
      {
        if (*it == TypeAssertion)
          it = body->erase(it, std::next(it));
        else
          ++it;
      }
    }

    return true;
  }

  PassDef infer()
  {
    PassDef p{"infer", wfPassInfer, dir::once, {}};

    p.post([](auto top) {
      // First pass: process all functions, skip TypeVar error checks.
      // Caller-side type propagation fills TypeVar params/fields in
      // anonymous class methods.
      Nodes deferred;

      top->traverse([&](auto node) {
        if (node != Function)
          return node == Top || node == ClassDef || node == ClassBody ||
            node == Lib || node == Symbols;

        process_function(node, top, false);

        // Collect functions that still have unresolved TypeVar types.
        if (has_typevar(node))
          deferred.push_back(node);

        return false;
      });

      // Second pass: iteratively re-process deferred functions until
      // no more progress is made. Each iteration may resolve FieldDef
      // types that allow sibling methods to resolve in the next
      // iteration (e.g., apply resolves field types from the callee's
      // signature, then create resolves its params from FieldDefs).
      // We reset return types to TypeVar before each re-process so
      // that return type inference runs fresh with updated env data.
      //
      // Progress is tracked by counting how many functions still have
      // unresolved TypeVar types (params, fields, or return). The loop
      // terminates when no further resolution occurs or after at most
      // N iterations (bounded by the deferred list size).
      size_t prev_tv_count = deferred.size();

      for (size_t iter = 0; iter < deferred.size(); iter++)
      {
        for (auto& func : deferred)
        {
          // Reset the return type to TypeVar so inference re-runs
          // with the latest FieldDef/param types.
          auto old_ret = func / Type;
          func->replace(old_ret, make_type());

          process_function(func, top, false);
        }

        // Count functions that still have unresolved TypeVars.
        size_t tv_count = 0;

        for (auto& func : deferred)
        {
          if (has_typevar(func))
            tv_count++;
        }

        if (tv_count == 0 || tv_count >= prev_tv_count)
          break;

        prev_tv_count = tv_count;
      }

      // Final check: error on any remaining unresolved TypeVars.
      for (auto& func : deferred)
      {
        if (!has_typevar(func))
          continue;

        process_function(func, top, true);
      }

      return 0;
    });

    return p;
  }
}
