#include "subtype.h"

namespace vc
{
  // Build SubType implications from a TypeName reference's path.
  // Each TypeArg on a NameElement is paired with the corresponding
  // TypeParam of that scope, creating bidirectional SubType
  // implications to express equality (TypeParam = TypeArg).
  SequentCtx build_typearg_ctx(const SequentCtx& base, const Node& name)
  {
    SequentCtx new_ctx = base;
    Node scope = base.scope;

    for (size_t i = 0; i < name->size(); i++)
    {
      auto elem = name->at(i);
      auto defs = scope->look((elem / Ident)->location());
      assert(!defs.empty());
      scope = defs.front();

      auto type_args = elem / TypeArgs;
      if (type_args->empty())
        continue;

      auto type_params = scope / TypeParams;
      auto ta_it = type_args->begin();
      auto tp_it = type_params->begin();

      while (ta_it != type_args->end() && tp_it != type_params->end())
      {
        if (*tp_it == TypeParam)
        {
          // Build a FQ TypeName for this TypeParam.
          auto make_fq_tp = [&]() {
            Node fq_tp = TypeName;
            for (size_t j = 0; j <= i; j++)
            {
              fq_tp << (NameElement << clone(name->at(j) / Ident) << TypeArgs);
            }
            fq_tp << (NameElement << clone((*tp_it) / Ident) << TypeArgs);
            return fq_tp;
          };

          // Bidirectional implications:
          // TypeParam <: TypeArg and TypeArg <: TypeParam.
          new_ctx.implies.push_back(
            SubType << (Type << make_fq_tp()) << clone(*ta_it));
          new_ctx.implies.push_back(
            SubType << clone(*ta_it) << (Type << make_fq_tp()));
        }

        ++ta_it;
        ++tp_it;
      }
    }

    return new_ctx;
  }

  // Find a function in a ClassDef that matches the given signature:
  // same name, handedness, param count, and TypeParam count.
  Node find_matching_function(const Node& def, const Node& target_func)
  {
    auto target_name = (target_func / Ident)->location();
    auto target_hand = (target_func / Lhs)->type();
    auto target_arity = (target_func / Params)->size();
    auto target_tp_count = (target_func / TypeParams)->size();

    for (auto& child : *(def / ClassBody))
    {
      if (child != Function)
        continue;

      if (
        ((child / Ident)->location() == target_name) &&
        ((child / Lhs)->type() == target_hand) &&
        ((child / Params)->size() == target_arity) &&
        ((child / TypeParams)->size() == target_tp_count))
      {
        return child;
      }
    }

    return {};
  }

  // Build bidirectional SubType implications pairing the TypeParams of two
  // functions by position. Both functions must have the same TypeParam count.
  // The FQ TypeParam names are built relative to their respective enclosing
  // TypeName references (l_name / r_name).
  SequentCtx build_func_typeparam_ctx(
    const SequentCtx& base,
    const Node& l_name,
    const Node& l_func,
    const Node& r_name,
    const Node& r_func)
  {
    SequentCtx new_ctx = base;
    auto l_tps = l_func / TypeParams;
    auto r_tps = r_func / TypeParams;
    assert(l_tps->size() == r_tps->size());

    auto l_it = l_tps->begin();
    auto r_it = r_tps->begin();

    while (l_it != l_tps->end() && r_it != r_tps->end())
    {
      if ((*l_it == TypeParam) && (*r_it == TypeParam))
      {
        // FQ name: scope_path_from_name / func_ident / typeparam_ident
        auto make_fq = [](const Node& name, const Node& func, const Node& tp) {
          Node fq = TypeName;
          for (auto& elem : *name)
            fq << (NameElement << clone(elem / Ident) << TypeArgs);

          fq << (NameElement << clone(func / Ident) << TypeArgs);
          fq << (NameElement << clone(tp / Ident) << TypeArgs);
          return fq;
        };

        auto l_fq = make_fq(l_name, l_func, *l_it);
        auto r_fq = make_fq(r_name, r_func, *r_it);

        new_ctx.implies.push_back(
          SubType << (Type << clone(l_fq)) << (Type << clone(r_fq)));
        new_ctx.implies.push_back(
          SubType << (Type << std::move(r_fq)) << (Type << std::move(l_fq)));
      }

      ++l_it;
      ++r_it;
    }

    return new_ctx;
  }

  // Check whether l (any type) is a subtype of r (a shape).
  // For every function on the shape, l must have a matching function with
  // compatible signatures: parameter types contravariant (shape param <:
  // concrete param), return type covariant (concrete ret <: shape ret).
  bool check_shape_subtype(const SequentCtx& ctx, const Node& l, const Node& r)
  {
    auto l_def = find_def(ctx.scope, l);
    auto r_def = find_def(ctx.scope, r);
    assert(l_def && r_def);
    assert((r_def == ClassDef) && ((r_def / Shape) == Shape));

    // Build TypeArg↔TypeParam implications for both sides.
    auto new_ctx = build_typearg_ctx(ctx, l);
    new_ctx = build_typearg_ctx(new_ctx, r);

    // Add A <: S as an assumption to prevent infinite recursion when
    // checking function parameter types that reference the shape itself.
    new_ctx.implies.push_back(
      SubType << (Type << clone(l)) << (Type << clone(r)));

    // TypeSelf = l (the proposed concrete subtype). Bidirectional
    // implications make TypeSelf invariant with the concrete type.
    new_ctx.implies.push_back(
      SubType << (Type << TypeSelf) << (Type << clone(l)));
    new_ctx.implies.push_back(
      SubType << (Type << clone(l)) << (Type << TypeSelf));

    // Every function on the shape must have a compatible match on l.
    for (auto& shape_func : *(r_def / ClassBody))
    {
      if (shape_func != Function)
        continue;

      auto match = find_matching_function(l_def, shape_func);
      if (!match)
        return false;

      // Build TypeParam pairing implications for function-level TypeParams.
      auto func_ctx =
        build_func_typeparam_ctx(new_ctx, l, match, r, shape_func);

      // Check parameter types (contravariant): shape param <: concrete param.
      auto shape_params = shape_func / Params;
      auto match_params = match / Params;
      auto sp_it = shape_params->begin();
      auto mp_it = match_params->begin();

      while (sp_it != shape_params->end() && mp_it != match_params->end())
      {
        if (!Subtype(func_ctx, (*sp_it) / Type, (*mp_it) / Type))
          return false;

        ++sp_it;
        ++mp_it;
      }

      // Check return type (covariant): concrete ret <: shape ret.
      if (!Subtype(func_ctx, match / Type, shape_func / Type))
        return false;
    }

    return true;
  }

  // Check whether two shapes have conflicting functions. Two shapes conflict
  // if they share a function with the same name/arity/hand/TypeParam count
  // but incompatible types — meaning no concrete type could satisfy both.
  // A shared function conflicts if any parameter pair is contradictory
  // (neither is a subtype of the other) or the return types conflict.
  bool
  shape_functions_conflict(const SequentCtx& ctx, const Node& l, const Node& r)
  {
    auto l_def = find_def(ctx.scope, l);
    auto r_def = find_def(ctx.scope, r);
    assert(l_def && r_def);
    assert((l_def / Shape) == Shape);
    assert((r_def / Shape) == Shape);

    // Build TypeArg↔TypeParam implications for both sides.
    auto new_ctx = build_typearg_ctx(ctx, l);
    new_ctx = build_typearg_ctx(new_ctx, r);

    for (auto& l_func : *(l_def / ClassBody))
    {
      if (l_func != Function)
        continue;

      auto r_func = find_matching_function(r_def, l_func);
      if (!r_func)
        continue;

      // Same signature identity — check for type conflicts.
      auto func_ctx = build_func_typeparam_ctx(new_ctx, l, l_func, r, r_func);

      // For a concrete type to satisfy both shapes, it would need a function
      // whose param types are supertypes of BOTH shapes' param types, and
      // whose return type is a subtype of BOTH shapes' return types.
      // Parameters conflict if neither shape's param is a subtype of the
      // other (no common supertype exists by this approximation).
      auto l_params = l_func / Params;
      auto r_params = r_func / Params;
      auto lp_it = l_params->begin();
      auto rp_it = r_params->begin();

      while (lp_it != l_params->end() && rp_it != r_params->end())
      {
        auto lt = (*lp_it) / Type;
        auto rt = (*rp_it) / Type;

        if (!Subtype(func_ctx, lt, rt) && !Subtype(func_ctx, rt, lt))
        {
          return true;
        }

        ++lp_it;
        ++rp_it;
      }

      // Return types conflict if neither is a subtype of the other
      // (no common subtype exists by this approximation).
      auto l_ret = l_func / Type;
      auto r_ret = r_func / Type;

      if (!Subtype(func_ctx, l_ret, r_ret) && !Subtype(func_ctx, r_ret, l_ret))
      {
        return true;
      }
    }

    return false;
  }
}
