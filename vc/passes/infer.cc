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
        << (TypeName
            << (NameElement << (Ident ^ "_builtin") << TypeArgs)
            << (NameElement << (Ident ^ "array")
                            << (TypeArgs << primitive_type(U8))));
    }

    // Navigate a FQ TypeName/FuncName from Top to its definition node.
    // Returns empty Node if navigation fails.
    // Not suitable for FuncName final resolution when overloads exist —
    // use resolve_call() for Call sites.
    Node find_def(Node top, const Node& name)
    {
      assert(name->in({FuncName, TypeName}));
      Node def = top;

      for (auto& elem : *name)
      {
        assert(elem == NameElement);
        auto defs = def->look((elem / Ident)->location());

        if (defs.empty())
          return {};

        def = defs.front();
      }

      return def;
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

    // Entry for a local variable in the type environment.
    struct LocalTypeInfo
    {
      Node type; // Source-level Type node (e.g., Type << TypeName << ...)
      bool is_default; // True if type came from Const default (U64/F64)
      bool is_fixed; // True if from Param or explicit Var annotation
      Node const_node; // If from a Const, the Const stmt (for refinement)
    };

    using TypeEnv = std::map<Location, LocalTypeInfo>;

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

    // Try to infer TypeArgs and refine Const types at a Call site.
    void infer_call(Node call, TypeEnv& env, Node top)
    {
      assert(call == Call);
      auto funcname = call / FuncName;
      auto args = call / Args;
      auto hand = (call / Lhs)->type();

      // Navigate the FuncName from Top, collecting scope info.
      std::vector<ScopeInfo> scopes;
      Node def = top;

      for (auto it = funcname->begin(); it != funcname->end(); ++it)
      {
        auto& elem = *it;
        assert(elem == NameElement);
        auto defs = def->look((elem / Ident)->location());

        if (defs.empty())
          return;

        bool is_last = (it + 1 == funcname->end());

        if (is_last)
        {
          // Filter for the correct Function overload.
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
            return;
        }
        else
        {
          def = defs.front();

          if (def == TypeParam)
            return;
        }

        scopes.push_back({elem, def});
      }

      if (scopes.empty())
        return;

      auto func_def = scopes.back().def;
      assert(func_def == Function);
      auto params = func_def / Params;

      if (params->size() != args->size())
        return;

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
      NodeMap<LocalTypeInfo> constraints;

      if (needs_inference)
      {
        for (size_t i = 0; i < params->size(); i++)
        {
          auto param = params->at(i);
          auto arg_node = args->at(i);
          auto formal_type = param / Type;

          auto tp_def = direct_typeparam(top, formal_type);

          if (!tp_def)
            continue;

          auto arg_src = arg_node / Rhs;
          auto it = env.find(arg_src->location());

          if (it == env.end())
            continue;

          auto& arg_info = it->second;
          auto existing = constraints.find(tp_def);

          if (existing == constraints.end())
          {
            constraints[tp_def] = {
              clone(arg_info.type), arg_info.is_default, false, {}};
          }
          else if (existing->second.is_default && !arg_info.is_default)
          {
            // Non-default type wins over default.
            existing->second = {clone(arg_info.type), false, false, {}};
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

        if (current_prim == expected_prim)
          continue;

        // Refine only within compatible domains.
        bool compatible = (current_prim->in(integer_types) &&
                           expected_prim->in(integer_types)) ||
          (current_prim->in(float_types) && expected_prim->in(float_types));

        if (!compatible)
          continue;

        refine_const(env, arg_info.const_node, expected_prim->type());
      }

      // Record the call's result type in the env.
      auto ret_type = func_def / Type;
      Node result_type;

      // If the return type references a TypeParam, substitute it.
      auto tp_def = direct_typeparam(top, ret_type);

      if (tp_def)
      {
        auto find = subst.find(tp_def);

        if (find != subst.end())
          result_type = clone(find->second);
      }
      else
      {
        // Use the return type directly (e.g., a concrete primitive).
        result_type = clone(ret_type);
      }

      if (result_type)
      {
        auto dst = call / LocalId;
        env[dst->location()] = {result_type, false, false, {}};
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

        // Initialize type env from function parameters.
        auto params = node / Params;

        for (auto& pd : *params)
        {
          assert(pd == ParamDef);
          auto ident = pd / Ident;
          auto type = pd / Type;
          env[ident->location()] = {clone(type), false, true, {}};
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
                is_default ? stmt : Node{}};
            }
            else if (stmt == ConstStr)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {string_type(), false, false, {}};
            }
            else if (stmt == Convert)
            {
              env[(stmt / LocalId)->location()] = {
                primitive_type((stmt / Type)->type()), false, false, {}};
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
                // Propagate source type. Carry const_node for refinement.
                env[dst->location()] = {
                  clone(src_it->second.type),
                  src_it->second.is_default,
                  false,
                  src_it->second.const_node};
              }
            }
            else if (stmt == Var)
            {
              auto ident = stmt / Ident;
              auto type = stmt / Type;

              // Only record if explicitly annotated (not TypeVar).
              if (type->front() != TypeVar)
                env[ident->location()] = {clone(type), false, true, {}};
            }
            else if (stmt == New)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {clone(stmt / Type), false, false, {}};

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

                      if (current_prim == expected_prim)
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
                  clone(it->second.type), false, false, {}};
            }
            else if (stmt->in({Eq, Ne, Lt, Le, Gt, Ge}))
            {
              // Comparison result is Bool.
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(Bool), false, false, {}};
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
                  clone(it->second.type), false, false, {}};
            }
            else if (stmt->in({IsInf, IsNaN, Not}))
            {
              // Bool-producing unops.
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(Bool), false, false, {}};
            }
            else if (stmt == Bits)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(U64), false, false, {}};
            }
            else if (stmt == Len)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(USize), false, false, {}};
            }
            else if (stmt->in({Const_E, Const_Pi, Const_Inf, Const_NaN}))
            {
              // Float constants: F64.
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(F64), false, false, {}};
            }
            else if (stmt == Typetest)
            {
              auto dst = stmt / LocalId;
              env[dst->location()] = {primitive_type(Bool), false, false, {}};
            }
            else if (stmt == Call)
            {
              // Phase 2+3: type arg inference and literal refinement.
              infer_call(stmt, env, top);
            }
            // All other statements (CallDyn, Lookup, FFI, When, etc.):
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

                if (current_prim != expected_prim)
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
