#pragma once

#include "lang.h"
#include "typevar.h"

#include <vbcc/sequent.h>

namespace vc
{
  bool
  shape_functions_conflict(const SequentCtx& ctx, const Node& l, const Node& r);

  // Internal to the Subtype axiom table — do not call directly.
  // Use Subtype(ctx, l, r) instead.
  bool check_shape_subtype(const SequentCtx& ctx, const Node& l, const Node& r);

  // TypeVar atom helper. Consults ctx.constraint_store and ctx.mode:
  //   - Mode::Emit + store present: emit add_lower / add_upper (or
  //     unify when both sides are TypeVars) and return {true, true}
  //     (proof delayed; constraint recorded).
  //   - Mode::Query or no store: return {true, false} (no emission;
  //     caller falls back to default axiom semantics).
  // The first bool is "the lambda handled this case at all" — true
  // means do not invoke the default axiom; false means delegate.
  // The second bool, valid only when the first is true, is the
  // axiom's return value.
  std::pair<bool, bool>
  try_typevar_atom(const SequentCtx& ctx, const Node& l, const Node& r);

  // Wraps an existing atom axiom with TypeVar emission preflight.
  // If either l or r is a TypeVar OR a TypeName resolving to a
  // TypeParam, defer to try_typevar_atom; if it handles the case,
  // return its result. Otherwise the wrapped axiom runs unchanged.
  // Used for primitive / TupleType / TypeName / TypeVar / TypeSelf
  // provability axioms in Subtype.
  inline Axiom with_typevar(Axiom inner)
  {
    return [inner](const SequentCtx& ctx, Node& l, Node& r) -> bool {
      auto [handled, ok] = try_typevar_atom(ctx, l, r);
      if (handled)
        return ok;
      return inner(ctx, l, r);
    };
  }

  inline const SequentCalculus Subtype{
    {Type},
    {Union, WhereOr},
    {Isect, WhereAnd},
    {WhereNot},
    {SubType},
    {
      TupleType >> with_typevar(
        [](const SequentCtx& ctx, Node& l, Node& r) {
          // Tuples must be the same arity and each element must be a subtype.
          return (l == TupleType) &&
            std::equal(
                   l->begin(),
                   l->end(),
                   r->begin(),
                   r->end(),
                   [&](auto& t, auto& u) { return Subtype(ctx, t, u); });
        }),

      TypeName >> with_typevar(
        [](const SequentCtx& ctx, Node& l, Node& r) {
          if ((l != TypeName) || (r != TypeName))
            return false;

          // Navigate both TypeNames to their definition sites.
          auto l_def = find_def(ctx.scope, l);
          auto r_def = find_def(ctx.scope, r);
          assert(l_def);
          assert(r_def);

          // TypeParams can only prove subtype of themselves.
          if ((l_def == TypeParam) || (r_def == TypeParam))
            return l_def->equals(r_def);

          if ((l_def == TypeAlias) || (r_def == TypeAlias))
          {
            // Expand the alias: substitute its TypeParams with the
            // TypeName's TypeArgs and recurse with the concrete type.
            // Generic instantiation by direct substitution — no implications.
            // Note: assumes alias chains are acyclic (guaranteed by the ident
            // pass). Cyclic aliases would cause infinite recursion.
            if (l_def == TypeAlias)
            {
              auto subst = build_subst_from_typename(ctx.scope, l);
              auto expanded = apply_subst(ctx.scope, l_def / Type, subst);
              return Subtype(ctx, expanded, r);
            }
            else
            {
              auto subst = build_subst_from_typename(ctx.scope, r);
              auto expanded = apply_subst(ctx.scope, r_def / Type, subst);
              return Subtype(ctx, l, expanded);
            }
          }

          // Shape (structural) subtyping: any type is a subtype of a shape
          // if it provides all the shape's functions with compatible
          // signatures. Check RHS shape first; if LHS is also a shape, the
          // same structural check applies.
          if ((r_def / Shape) == Shape)
          {
            return check_shape_subtype(ctx, l, r);
          }

          if ((l_def / Shape) == Shape)
          {
            // A shape on the LHS can only prove subtype of a nominal type
            // if the nominal type is also a shape — but that case was
            // handled above (r_def would be a shape). So a shape is never
            // a subtype of a concrete nominal type.
            return false;
          }

          // Definition sites must be the same node.
          if (l_def != r_def)
            return false;

          // Check all TypeArgs at every NameElement are invariant.
          if (l->size() != r->size())
            return false;

          return std::equal(
            l->begin(),
            l->end(),
            r->begin(),
            r->end(),
            [&](auto& le, auto& re) {
              auto l_ta = le / TypeArgs;
              auto r_ta = re / TypeArgs;

              return std::equal(
                l_ta->begin(),
                l_ta->end(),
                r_ta->begin(),
                r_ta->end(),
                [&](auto& t, auto& u) { return Subtype.invariant(ctx, t, u); });
            });
        }),

      Dyn >> AxiomTrue,
      None >> with_typevar(AxiomEq),
      Bool >> with_typevar(AxiomEq),
      I8 >> with_typevar(AxiomEq),
      I16 >> with_typevar(AxiomEq),
      I32 >> with_typevar(AxiomEq),
      I64 >> with_typevar(AxiomEq),
      U8 >> with_typevar(AxiomEq),
      U16 >> with_typevar(AxiomEq),
      U32 >> with_typevar(AxiomEq),
      U64 >> with_typevar(AxiomEq),
      ISize >> with_typevar(AxiomEq),
      USize >> with_typevar(AxiomEq),
      ILong >> with_typevar(AxiomEq),
      ULong >> with_typevar(AxiomEq),
      F32 >> with_typevar(AxiomEq),
      F64 >> with_typevar(AxiomEq),
      DefaultInt >> AxiomEq,
      DefaultFloat >> AxiomEq,
      TypeSelf >> with_typevar(AxiomEq),
      TypeVar >> with_typevar(AxiomEq),
    },
    {
      // TypeSelf is always bound through implications (TypeSelf <: T and
      // T <: TypeSelf). It should never trigger contradiction detection
      // because the implications may not yet be decomposed when the atom
      // is checked.
      TypeSelf >> AxiomFalse,

      // DefaultInt/DefaultFloat are unresolved literals that could become
      // any primitive. They never contradict any type.
      DefaultInt >> AxiomFalse,
      DefaultFloat >> AxiomFalse,

      // TypeVar is an unresolved type parameter. Like defaults, it
      // never contradicts — it could become anything.
      TypeVar >> AxiomFalse,

      TypeName >>
        [](const SequentCtx& ctx, Node& l, Node& r) {
          // Type variables (TypeParam) and unexpanded aliases (TypeAlias)
          // never contradict anything, since their concrete type is
          // unknown at this point.
          if (r != TypeName)
            return false;

          auto r_def = find_def(ctx.scope, r);
          if (!r_def || r_def->type().in({TypeParam, TypeAlias}))
            return false;

          if (l != TypeName)
          {
            // l is a non-TypeName atom (primitive, tuple, etc.).
            // Shapes never contradict non-TypeName atoms (the atom might
            // satisfy the shape). Concrete ClassDefs do contradict.
            if ((r_def == ClassDef) && ((r_def / Shape) == Shape))
              return false;

            return true;
          }

          auto l_def = find_def(ctx.scope, l);
          if (!l_def || l_def->type().in({TypeParam, TypeAlias}))
            return false;

          bool l_shape = (l_def == ClassDef) && ((l_def / Shape) == Shape);
          bool r_shape = (r_def == ClassDef) && ((r_def / Shape) == Shape);

          if (r_shape && !l_shape)
          {
            // Concrete type vs shape: contradicts if the concrete type
            // does not satisfy the shape.
            return !check_shape_subtype(ctx, l, r);
          }

          if (l_shape && !r_shape)
          {
            // Shape vs concrete type (symmetric call).
            return !check_shape_subtype(ctx, r, l);
          }

          if (l_shape && r_shape)
          {
            // Two shapes contradict only if they have a conflicting
            // function (same name/arity/hand but incompatible types,
            // making it impossible for any concrete type to satisfy both).
            return shape_functions_conflict(ctx, l, r);
          }

          // Both are concrete ClassDefs. Different defs contradict.
          return l_def != r_def;
        },
    }};
}
