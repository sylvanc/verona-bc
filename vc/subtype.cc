#include "subtype.h"

namespace vc
{
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

  bool definitely_not_subtype(const SequentCtx& ctx, Node l, Node r)
  {
    if (!l || !r)
      return false;

    if (l == Type)
      l = l->front();

    if (r == Type)
      r = r->front();

    if (!l || !r)
      return false;

    if (
      l->in(
        {Dyn, TypeSelf, TypeVar, Union, Isect, WhereOr, WhereAnd, WhereNot}) ||
      r->in(
        {Dyn, TypeSelf, TypeVar, Union, Isect, WhereOr, WhereAnd, WhereNot}))
      return false;

    if ((l == TupleType) || (r == TupleType))
    {
      if ((l != TupleType) || (r != TupleType))
        return true;

      if (l->size() != r->size())
        return true;

      auto li = l->begin();
      auto ri = r->begin();

      while (li != l->end() && ri != r->end())
      {
        if (
          definitely_not_subtype(ctx, *li, *ri) ||
          definitely_not_subtype(ctx, *ri, *li))
          return true;

        ++li;
        ++ri;
      }

      return false;
    }

    auto primitive = [](const Node& n) {
      return n->in(
        {None,
         Bool,
         I8,
         I16,
         I32,
         I64,
         U8,
         U16,
         U32,
         U64,
         ISize,
         USize,
         ILong,
         ULong,
         F32,
         F64,
         DefaultInt,
         DefaultFloat});
    };

    if (primitive(l) || primitive(r))
      return l->type() != r->type();

    if ((l != TypeName) || (r != TypeName))
      return false;

    auto l_def = find_def(ctx.scope, l);
    auto r_def = find_def(ctx.scope, r);

    if (!l_def || !r_def)
      return false;

    if (l_def == TypeAlias)
    {
      // Expand the alias: substitute its TypeParams with the TypeName's
      // TypeArgs and recurse with the concrete type.
      auto subst = build_subst_from_typename(ctx.scope, l);
      auto expanded = apply_subst(ctx.scope, l_def / Type, subst);
      return definitely_not_subtype(ctx, expanded, Type << clone(r));
    }

    if (r_def == TypeAlias)
    {
      auto subst = build_subst_from_typename(ctx.scope, r);
      auto expanded = apply_subst(ctx.scope, r_def / Type, subst);
      return definitely_not_subtype(ctx, Type << clone(l), expanded);
    }

    if (l_def->type().in({TypeParam}) || r_def->type().in({TypeParam}))
      return false;

    bool l_shape = (l_def == ClassDef) && ((l_def / Shape) == Shape);
    bool r_shape = (r_def == ClassDef) && ((r_def / Shape) == Shape);

    if (l_shape || r_shape)
      return false;

    if (l_def != r_def)
      return true;

    if (l->size() != r->size())
      return true;

    auto le = l->begin();
    auto re = r->begin();

    while (le != l->end() && re != r->end())
    {
      auto l_ta = (*le) / TypeArgs;
      auto r_ta = (*re) / TypeArgs;

      if (l_ta->size() != r_ta->size())
        return true;

      auto lti = l_ta->begin();
      auto rti = r_ta->begin();

      while (lti != l_ta->end() && rti != r_ta->end())
      {
        if (
          definitely_not_subtype(ctx, *lti, *rti) ||
          definitely_not_subtype(ctx, *rti, *lti))
          return true;

        ++lti;
        ++rti;
      }

      ++le;
      ++re;
    }

    return false;
  }

  // Build bidirectional SubType implications pairing the TypeParams of two
  // functions by position. Both functions must have the same TypeParam count.
  // Function-level TypeParams are universally quantified — they need to be
  // matched (alpha-equivalence) rather than instantiated. The bidirectional
  // implications model this for small N (typically 1-2 function TypeParams).
  // Long-term: replace with skolemization (rename to fresh shared rigid).
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
  //
  // Class-level TypeParams are bound by direct substitution (System F):
  // L's TypeArgs substitute into the concrete function signatures; R's
  // TypeArgs substitute into the shape signatures. TypeSelf in shape
  // signatures is replaced with L. After substitution, recursive Subtype
  // calls compare fully concrete types — no TypeParam refs survive.
  bool check_shape_subtype(const SequentCtx& ctx, const Node& l, const Node& r)
  {
    // Coinductive check: if l <: r is already assumed in the
    // assumptions (not implies), return true. This handles recursive
    // shape types where a shape function returns the same shape type.
    // Assumptions are visible to axiom callbacks but are never
    // decomposed by the sequent calculus, preventing infinite
    // re-decomposition of the coinductive hypothesis.
    for (auto& imp : ctx.assumptions)
    {
      if (imp != SubType || imp->size() != 2)
        continue;

      auto imp_l = imp->front();
      auto imp_r = imp->back();
      auto l_copy = l;
      auto r_copy = r;

      if (
        imp_l == Type && imp_r == Type && imp_l->size() == 1 &&
        imp_r->size() == 1 && imp_l->front()->equals(l_copy) &&
        imp_r->front()->equals(r_copy))
        return true;
    }

    auto l_def = find_def(ctx.scope, l);
    auto r_def = find_def(ctx.scope, r);
    if (!l_def || !r_def)
      return false;
    assert((r_def == ClassDef) && ((r_def / Shape) == Shape));

    // Build per-side substitutions for class-level TypeParams.
    auto subst_l = build_subst_from_typename(ctx.scope, l);
    auto subst_r = build_subst_from_typename(ctx.scope, r);

    // L wrapped as a Type — used as the TypeSelf binding for shape sigs.
    Node l_as_type = Type << clone(l);

    // Coinductive assumption: l <: r. Stored in `assumptions` (not
    // `implies`) so the sequent calculus never decomposes it.
    SequentCtx new_ctx = ctx;
    new_ctx.assumptions.push_back(
      SubType << (Type << clone(l)) << (Type << clone(r)));

    // Every function on the shape must have a compatible match on l.
    for (auto& shape_func : *(r_def / ClassBody))
    {
      if (shape_func != Function)
        continue;

      auto match = find_matching_function(l_def, shape_func);
      if (!match)
        return false;

      // Function-level TypeParams: pair them via implications (still small N).
      auto func_ctx =
        build_func_typeparam_ctx(new_ctx, l, match, r, shape_func);

      // Substitute TypeSelf with L on BOTH sides (TypeSelf in either
      // signature represents "the implementing type", which is L). Then
      // substitute class-level TypeParams: shape sigs use R's subst, match
      // sigs use L's subst. After substitution, recursive Subtype calls
      // compare fully concrete types — no TypeSelf or TypeParam refs survive.
      auto subst_shape_type = [&](const Node& t) {
        Node bound = substitute_typeself(t, l_as_type);
        return apply_subst(ctx.scope, bound, subst_r);
      };
      auto subst_match_type = [&](const Node& t) {
        Node bound = substitute_typeself(t, l_as_type);
        return apply_subst(ctx.scope, bound, subst_l);
      };

      // Check parameter types (contravariant): shape param <: concrete param.
      auto shape_params = shape_func / Params;
      auto match_params = match / Params;
      auto sp_it = shape_params->begin();
      auto mp_it = match_params->begin();

      while (sp_it != shape_params->end() && mp_it != match_params->end())
      {
        auto sp_t = subst_shape_type((*sp_it) / Type);
        auto mp_t = subst_match_type((*mp_it) / Type);

        if (
          definitely_not_subtype(func_ctx, sp_t, mp_t) ||
          !Subtype(func_ctx, sp_t, mp_t))
          return false;

        ++sp_it;
        ++mp_it;
      }

      // Check return type (covariant): concrete ret <: shape ret.
      auto shape_ret = subst_shape_type(shape_func / Type);
      auto match_ret = subst_match_type(match / Type);

      if (
        definitely_not_subtype(func_ctx, match_ret, shape_ret) ||
        !Subtype(func_ctx, match_ret, shape_ret))
        return false;
    }

    return true;
  }

  // Check whether two shapes have conflicting functions. Two shapes conflict
  // if they share a function with the same name/arity/hand/TypeParam count
  // but incompatible types — meaning no concrete type could satisfy both.
  // A shared function conflicts if any parameter pair is contradictory
  // (neither is a subtype of the other) or the return types conflict.
  //
  // Class-level TypeParams are bound by direct substitution. For shape
  // conflict, no concrete TypeSelf binding is meaningful; we leave TypeSelf
  // as-is in both sides' signatures (it represents the same hypothetical
  // implementing type on both sides).
  bool
  shape_functions_conflict(const SequentCtx& ctx, const Node& l, const Node& r)
  {
    auto l_def = find_def(ctx.scope, l);
    auto r_def = find_def(ctx.scope, r);
    assert(l_def && r_def);
    assert((l_def / Shape) == Shape);
    assert((r_def / Shape) == Shape);

    // Build per-side substitutions for class-level TypeParams.
    auto subst_l = build_subst_from_typename(ctx.scope, l);
    auto subst_r = build_subst_from_typename(ctx.scope, r);

    for (auto& l_func : *(l_def / ClassBody))
    {
      if (l_func != Function)
        continue;

      auto r_func = find_matching_function(r_def, l_func);
      if (!r_func)
        continue;

      // Function-level TypeParam pairing for matched function.
      auto func_ctx = build_func_typeparam_ctx(ctx, l, l_func, r, r_func);

      auto subst_l_type = [&](const Node& t) {
        return apply_subst(ctx.scope, t, subst_l);
      };
      auto subst_r_type = [&](const Node& t) {
        return apply_subst(ctx.scope, t, subst_r);
      };

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
        auto lt = subst_l_type((*lp_it) / Type);
        auto rt = subst_r_type((*rp_it) / Type);

        if (!Subtype(func_ctx, lt, rt) && !Subtype(func_ctx, rt, lt))
        {
          return true;
        }

        ++lp_it;
        ++rp_it;
      }

      // Return types conflict if neither is a subtype of the other
      // (no common subtype exists by this approximation).
      auto l_ret = subst_l_type(l_func / Type);
      auto r_ret = subst_r_type(r_func / Type);

      if (!Subtype(func_ctx, l_ret, r_ret) && !Subtype(func_ctx, r_ret, l_ret))
      {
        return true;
      }
    }

    return false;
  }
}
