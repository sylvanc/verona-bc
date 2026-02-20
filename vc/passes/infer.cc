#include "../lang.h"

namespace vc
{
  namespace
  {
    // Map from builtin primitive name to IR token.
    // Ptr is included for type env tracking even though it's not refinable
    // (not in the integer or float domain).
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
      {"ptr", Ptr},
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

    // Build a source-level Type node for _builtin::array[_builtin::u8].
    // Creates fresh nodes on each call.
    Node string_type()
    {
      return Type
        << (TypeName << (NameElement << (Ident ^ "_builtin") << TypeArgs)
                     << (NameElement << (Ident ^ "array")
                                     << (TypeArgs << primitive_type(U8))));
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
      bool is_fixed; // True if from Param or explicit Var annotation
      Node const_node; // If from a Const, the Const stmt (for refinement)
      Node call_node; // If from a default-inferred Call, the Call stmt
    };

    using TypeEnv = std::map<Location, LocalTypeInfo>;

    // Forward declaration for use in extract_constraints.
    Node
    apply_subst(Node top, const Node& type_node, const NodeMap<Node>& subst);

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
            existing->second = {actual_type, false, false, {}, {}};
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
          auto shape_tps = f_def / TypeParams;
          auto f_last_ta = f_inner->back() / TypeArgs;

          if (shape_tps->size() == f_last_ta->size())
          {
            NodeMap<Node> shape_to_formal;

            for (size_t i = 0; i < shape_tps->size(); i++)
              shape_to_formal[shape_tps->at(i)] = f_last_ta->at(i);

            // Map actual class TypeParams -> actual TypeArgs (concrete
            // types, e.g., range.A -> i32).
            auto actual_tps = a_def / TypeParams;
            auto a_last_ta = a_inner->back() / TypeArgs;

            if (actual_tps->size() == a_last_ta->size())
            {
              NodeMap<Node> actual_subst;

              for (size_t i = 0; i < actual_tps->size(); i++)
                actual_subst[actual_tps->at(i)] = a_last_ta->at(i);

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
                  auto formal_ret =
                    apply_subst(top, shape_ret, shape_to_formal);

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
    Node
    apply_subst(Node top, const Node& type_node, const NodeMap<Node>& subst)
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

      // FuncType, TypeVar, etc. - return as-is.
      return clone(type_node);
    }

    // Resolve the return type of a method on a receiver type.
    // Given a receiver type (e.g., wrapper[i32]), a method name, hand,
    // arity, and method-level TypeArgs, find the method definition
    // and return the substituted return type.
    Node resolve_method_return_type(
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
      auto class_tps = class_def / TypeParams;
      NodeMap<Node> subst;

      // Get TypeArgs from the last NameElement of the receiver TypeName.
      auto last_elem = inner->back();
      auto class_ta = last_elem / TypeArgs;

      if (class_ta->size() == class_tps->size())
      {
        for (size_t i = 0; i < class_tps->size(); i++)
          subst[class_tps->at(i)] = class_ta->at(i);
      }

      // Find the matching method in the class.
      auto class_body = class_def / ClassBody;
      auto method_name = method_ident->location().view();

      for (auto& child : *class_body)
      {
        if (child != Function)
          continue;

        if ((child / Ident)->location().view() != method_name)
          continue;

        if ((child / Lhs)->type() != hand)
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

        // Apply substitution to the return type.
        auto ret_type = child / Type;
        return apply_subst(top, ret_type, subst);
      }

      return {};
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

      // Must be a two-element path: _builtin::name.
      if (inner->size() != 2)
        return {};

      auto first_ident = (inner->front() / Ident)->location().view();
      auto second_ident = (inner->back() / Ident)->location().view();

      if (first_ident != "_builtin")
        return {};

      auto it = primitive_from_name.find(second_ident);

      if (it != primitive_from_name.end())
        return it->second;

      return {};
    }

    // Like extract_primitive but also handles union and intersection
    // types by scanning all components for primitives. Returns empty
    // if ambiguous (components with different primitive types found).
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

    // Return the default primitive type for a literal node.
    Node default_literal_type(const Node& lit)
    {
      if (lit->in({True, False}))
        return Bool;

      if (lit == None)
        return None;

      if (lit->in({Bin, Oct, Int, Hex}))
        return U64;

      if (lit->in({Float, HexFloat}))
        return F64;

      assert(false && "unhandled literal type in infer");
      return {};
    }

    // Refine a Const node's type to new_prim. Updates the AST and all
    // env entries sharing the same const_node.
    void refine_const(TypeEnv& env, Node const_node, const Token& new_prim)
    {
      assert(const_node == Const);

      // Modify the AST: replace the Type child.
      auto old_type = const_node / Type;
      Node new_type = new_prim;
      const_node->replace(old_type, new_type);

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

    // Scope information collected during FuncName navigation.
    struct ScopeInfo
    {
      Node name_elem; // NameElement in the FuncName
      Node def; // ClassDef/Function definition node
    };

    // Navigate a Call node's FuncName from top, collecting scope
    // definitions along the path. Returns the target Function def on
    // success, or an empty Node on failure.
    Node navigate_call(
      Node call, Node top, std::vector<ScopeInfo>& scopes)
    {
      assert(call == Call);
      auto funcname = call / FuncName;
      auto args = call / Args;
      auto hand = (call / Lhs)->type();

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
          // Filter for the correct Function overload by hand and arity.
          bool found = false;

          for (auto& d : defs)
          {
            if (d != Function)
              continue;

            if ((d / Lhs)->type() != hand)
              continue;

            if ((d / Params)->size() != args->size())
              continue;

            def = d;
            found = true;
            break;
          }

          if (!found)
            return {};
        }
        else
        {
          def = defs.front();

          if (def == TypeParam)
            return {};
        }

        scopes.push_back({elem, def});
      }

      if (scopes.empty())
        return {};

      auto func_def = scopes.back().def;
      assert(func_def == Function);
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
          auto shape_tps = exp_def / TypeParams;
          auto exp_last_ta = expected_inner->back() / TypeArgs;

          if (shape_tps->size() == exp_last_ta->size())
          {
            NodeMap<Node> shape_to_concrete;

            for (size_t i = 0; i < shape_tps->size(); i++)
              shape_to_concrete[shape_tps->at(i)] = exp_last_ta->at(i);

            auto shape_body = exp_def / ClassBody;
            auto class_body = ret_def / ClassBody;

            for (auto& shape_func : *shape_body)
            {
              if (shape_func != Function)
                continue;

              auto method_name =
                (shape_func / Ident)->location().view();
              auto mhand = (shape_func / Lhs)->type();
              auto arity = (shape_func / Params)->size();

              for (auto& class_func : *class_body)
              {
                if (class_func != Function)
                  continue;

                if (
                  (class_func / Ident)->location().view() !=
                  method_name)
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
                 << Node(prior_call / FuncName) << std::endl;

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
        auto it = env.find(arg_src->location());

        if (it == env.end())
          continue;

        auto& arg_info = it->second;

        if (!arg_info.is_default || !arg_info.const_node)
          continue;

        Node current_prim = arg_info.const_node / Type;

        if (current_prim->type() == expected_prim->type())
          continue;

        bool compatible = (current_prim->in(integer_types) &&
                           expected_prim->in(integer_types)) ||
          (current_prim->in(float_types) && expected_prim->in(float_types));

        if (!compatible)
          continue;

        refine_const(env, arg_info.const_node, expected_prim->type());
      }

      // Recompute the prior call's result type and update env.
      auto result_type = apply_subst(top, ret_type, subst);

      if (result_type)
      {
        auto dst = prior_call / LocalId;
        env[dst->location()] = {result_type, false, false, {}, {}};
      }
    }

    // Try to infer TypeArgs and refine Const types at a Call site.
    void infer_call(Node call, TypeEnv& env, Node top)
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

        // Check if actual arg is a refinable Const.
        auto arg_src = arg_node / Rhs;
        auto it = env.find(arg_src->location());

        if (it == env.end())
          continue;

        auto& arg_info = it->second;

        if (!arg_info.is_default || !arg_info.const_node)
          continue;

        Node current_prim = arg_info.const_node / Type;

        if (current_prim->type() == expected_prim->type())
          continue;

        // Refine only within compatible domains.
        bool compatible = (current_prim->in(integer_types) &&
                           expected_prim->in(integer_types)) ||
          (current_prim->in(float_types) && expected_prim->in(float_types));

        if (!compatible)
          continue;

        refine_const(env, arg_info.const_node, expected_prim->type());
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

        if (!arg_info.is_default || !arg_info.call_node)
          continue;

        // Save call_node before backward_refine_call potentially
        // invalidates the arg_info reference by updating env.
        auto prior_call_node = arg_info.call_node;

        // Compute the expected type for this arg from formal + subst.
        auto expected = apply_subst(top, formal_type, subst);

        if (!expected)
          continue;

        backward_refine_call(prior_call_node, expected, env, top);

        // Update the arg's env entry to reflect the refined type.
        auto prior_dst = prior_call_node / LocalId;
        auto updated_it = env.find(prior_dst->location());

        if (updated_it != env.end())
        {
          env[arg_src->location()] = {
            clone(updated_it->second.type),
            updated_it->second.is_default,
            false,
            updated_it->second.const_node,
            updated_it->second.call_node};
        }
      }
    }
  }

  PassDef infer()
  {
    PassDef p{"infer", wfPassInfer, dir::once, {}};

    p.post([](auto top) {
      top->traverse([&](auto node) {
        if (node != Function)
          return node == Top || node == ClassDef || node == ClassBody;

        TypeEnv env;
        std::map<Location, Node> lookup_stmts;

        // Initialize type env from function parameters.
        auto params = node / Params;

        for (auto& pd : *params)
        {
          assert(pd == ParamDef);
          auto ident = pd / Ident;
          auto type = pd / Type;
          env[ident->location()] = {clone(type), false, true, {}, {}};
        }

        // Single forward pass over all labels:
        // - Phase 1: assign default types to Const nodes
        // - Phase 2: build type env, infer TypeArgs at Call sites
        // - Phase 3: refine Const types from Call expectations and Var
        //   annotations
        auto labels = node / Labels;

        for (auto& lbl : *labels)
        {
          for (auto& stmt : *(lbl / Body))
          {
            if (stmt == Const)
            {
              // Phase 1: Assign default type from literal.
              auto lit = stmt->back();
              Node type = default_literal_type(lit);
              auto dst = stmt->front();
              stmt->erase(stmt->begin(), stmt->end());
              stmt << dst << type << lit;

              // Record in type env.
              bool is_default = type->in({U64, F64});
              env[dst->location()] = {
                primitive_type(type->type()),
                is_default,
                false,
                is_default ? stmt : Node{}, {}};
            }
            else if (stmt == ConstStr)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {string_type(), false, false, {}, {}};
            }
            else if (stmt == Convert)
            {
              env[(stmt / LocalId)->location()] = {
                primitive_type((stmt / Type)->type()), false, false, {}, {}};
            }
            else if (stmt->in({Copy, Move}))
            {
              auto dst = stmt / LocalId;
              auto src = stmt / Rhs;
              auto dst_it = env.find(dst->location());
              auto src_it = env.find(src->location());

              if (
                dst_it != env.end() && dst_it->second.is_fixed &&
                src_it != env.end())
              {
                // Phase 3: dst has a fixed type (Var annotation).
                // If src is a refinable Const, refine to match dst.
                auto& dst_info = dst_it->second;
                auto& src_info = src_it->second;

                if (src_info.is_default && src_info.const_node)
                {
                  auto dst_prim = extract_primitive(dst_info.type);
                  auto src_prim = extract_primitive(src_info.type);

                  if (
                    dst_prim && src_prim &&
                    dst_prim->type() != src_prim->type())
                  {
                    bool compatible = (src_prim->in(integer_types) &&
                                       dst_prim->in(integer_types)) ||
                      (src_prim->in(float_types) && dst_prim->in(float_types));

                    if (compatible)
                      refine_const(env, src_info.const_node, dst_prim->type());
                  }
                }
              }
              else if (
                (dst_it == env.end() || !dst_it->second.is_fixed) &&
                src_it != env.end())
              {
                // Propagate source type. Carry const_node and call_node
                // for refinement.
                env[dst->location()] = {
                  clone(src_it->second.type),
                  src_it->second.is_default,
                  false,
                  src_it->second.const_node,
                  src_it->second.call_node};
              }
            }
            else if (stmt == Var)
            {
              auto ident = stmt / Ident;
              auto type = stmt / Type;

              // Only record if explicitly annotated (not TypeVar).
              if (type->front() != TypeVar)
                env[ident->location()] = {clone(type), false, true, {}, {}};
            }
            else if (stmt == New)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {clone(stmt / Type), false, false, {}, {}};

              // Refine Const literals used as New arguments based on
              // field types. E.g. `new {count = 0}` where count is
              // usize should refine the literal from u64 to usize.
              auto new_type = stmt / Type;
              auto inner = new_type->front();

              if (inner == TypeName)
              {
                auto class_def = find_def(top, inner);

                if (class_def && class_def == ClassDef)
                {
                  auto class_body = class_def / ClassBody;

                  for (auto& new_arg : *(stmt / NewArgs))
                  {
                    assert(new_arg == NewArg);
                    auto field_ident = new_arg / Ident;
                    auto arg_src = new_arg / Rhs;
                    auto it = env.find(arg_src->location());

                    if (it == env.end())
                      continue;

                    auto& arg_info = it->second;

                    if (!arg_info.is_default || !arg_info.const_node)
                      continue;

                    // Find the matching field in the class definition.
                    for (auto& child : *class_body)
                    {
                      if (child != FieldDef)
                        continue;

                      if (
                        (child / Ident)->location().view() !=
                        field_ident->location().view())
                        continue;

                      auto expected_prim = extract_primitive(child / Type);

                      if (!expected_prim)
                        break;

                      Node current_prim = arg_info.const_node / Type;

                      if (current_prim->type() == expected_prim->type())
                        break;

                      bool compatible = (current_prim->in(integer_types) &&
                                         expected_prim->in(integer_types)) ||
                        (current_prim->in(float_types) &&
                         expected_prim->in(float_types));

                      if (compatible)
                        refine_const(
                          env, arg_info.const_node, expected_prim->type());

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
              // Binop result has same type as LHS operand.
              auto dst = stmt / LocalId;
              auto lhs = stmt / Lhs;
              auto it = env.find(lhs->location());

              if (it != env.end())
                env[dst->location()] = {
                  clone(it->second.type), false, false, {}, {}};
            }
            else if (stmt->in({Eq, Ne, Lt, Le, Gt, Ge}))
            {
              // Comparison result is Bool.
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(Bool), false, false, {}, {}};
            }
            else if (stmt->in({Neg,  Abs,  Ceil, Floor, Exp,   Log,  Sqrt,
                               Cbrt, Sin,  Cos,  Tan,   Asin,  Acos, Atan,
                               Sinh, Cosh, Tanh, Asinh, Acosh, Atanh}))
            {
              // Unop: same type as source.
              auto dst = stmt / LocalId;
              auto src = stmt / Rhs;
              auto it = env.find(src->location());

              if (it != env.end())
                env[dst->location()] = {
                  clone(it->second.type), false, false, {}, {}};
            }
            else if (stmt->in({IsInf, IsNaN, Not}))
            {
              // Bool-producing unops.
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(Bool), false, false, {}, {}};
            }
            else if (stmt == Bits)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(U64), false, false, {}, {}};
            }
            else if (stmt == Len)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(USize), false, false, {}, {}};
            }
            else if (stmt->in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
            {
              // Float constants: F64.
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(F64), false, false, {}, {}};
            }
            else if (stmt == Typetest)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(Bool), false, false, {}, {}};
            }
            else if (stmt == Call)
            {
              // Phase 2+3: type arg inference and literal refinement.
              infer_call(stmt, env, top);
            }
            else if (stmt == Lookup)
            {
              // Resolve the return type of the looked-up method based
              // on the receiver's type. Store for CallDyn to use.
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
                  top,
                  src_it->second.type,
                  method_ident,
                  hand,
                  arity,
                  method_ta);

                if (ret_type)
                  env[dst->location()] = {ret_type, false, false, {}, {}};
              }

              lookup_stmts[dst->location()] = stmt;
            }
            else if (stmt == CallDyn)
            {
              auto dst = stmt / LocalId;
              auto src = stmt / Rhs;
              auto args = stmt / Args;

              // Refine default-typed args using non-default args.
              // E.g., sum + v where sum is u64 (default) and v is i32
              // should refine sum to i32.
              Node target_prim;

              for (auto& arg_node : *args)
              {
                auto arg_src = arg_node / Rhs;
                auto it = env.find(arg_src->location());

                if (it == env.end())
                  continue;

                if (it->second.is_default)
                  continue;

                auto prim = extract_callable_primitive(it->second.type);

                if (prim)
                {
                  target_prim = prim;
                  break;
                }
              }

              if (target_prim)
              {
                bool refined = false;

                for (auto& arg_node : *args)
                {
                  auto arg_src = arg_node / Rhs;
                  auto it = env.find(arg_src->location());

                  if (it == env.end())
                    continue;

                  auto& arg_info = it->second;

                  if (!arg_info.is_default || !arg_info.const_node)
                    continue;

                  Node current_prim = arg_info.const_node / Type;

                  if (current_prim->type() == target_prim->type())
                    continue;

                  bool compatible =
                    (current_prim->in(integer_types) &&
                     target_prim->in(integer_types)) ||
                    (current_prim->in(float_types) &&
                     target_prim->in(float_types));

                  if (compatible)
                  {
                    refine_const(
                      env, arg_info.const_node, target_prim->type());
                    refined = true;
                  }
                }

                // Re-resolve the Lookup with the updated receiver type.
                if (refined)
                {
                  auto lookup_it = lookup_stmts.find(src->location());

                  if (lookup_it != lookup_stmts.end())
                  {
                    auto lookup_node = lookup_it->second;
                    auto lookup_src = lookup_node / Rhs;
                    auto lookup_dst = lookup_node / LocalId;
                    auto hand = (lookup_node / Lhs)->type();
                    auto method_ident = lookup_node / Ident;
                    auto method_ta = lookup_node / TypeArgs;
                    auto arity =
                      from_chars_sep_v<size_t>(lookup_node / Int);
                    auto recv_it = env.find(lookup_src->location());

                    if (recv_it != env.end())
                    {
                      auto ret_type = resolve_method_return_type(
                        top,
                        recv_it->second.type,
                        method_ident,
                        hand,
                        arity,
                        method_ta);

                      if (ret_type)
                        env[lookup_dst->location()] = {
                          ret_type, false, false, {}, {}};
                    }
                  }
                }
              }

              // Propagate the Lookup result type to CallDyn result.
              auto src_it = env.find(src->location());

              if (src_it != env.end())
                env[dst->location()] = {
                  clone(src_it->second.type), false, false, {}, {}};
            }
            // All other statements (FFI, When, etc.):
            // result type unknown, don't record in env.
          }

          // Refine Const literals returned from this label based on
          // the function's declared return type.
          auto term = lbl / Return;

          if (term == Return)
          {
            auto ret_src = term / LocalId;
            auto it = env.find(ret_src->location());

            if (
              it != env.end() && it->second.is_default && it->second.const_node)
            {
              auto func_ret_type = node / Type;
              auto expected_prim = extract_primitive(func_ret_type);

              if (expected_prim)
              {
                Node current_prim = it->second.const_node / Type;

                if (current_prim->type() != expected_prim->type())
                {
                  bool compatible = (current_prim->in(integer_types) &&
                                     expected_prim->in(integer_types)) ||
                    (current_prim->in(float_types) &&
                     expected_prim->in(float_types));

                  if (compatible)
                    refine_const(
                      env, it->second.const_node, expected_prim->type());
                }
              }
            }
          }
        }

        return false;
      });

      return 0;
    });

    return p;
  }
}
