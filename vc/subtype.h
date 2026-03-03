#pragma once

#include "lang.h"

#include <vbcc/sequent.h>

namespace vc
{
  SequentCtx build_typearg_ctx(const SequentCtx& base, const Node& name);
  bool check_shape_subtype(const SequentCtx& ctx, const Node& l, const Node& r);
  bool
  shape_functions_conflict(const SequentCtx& ctx, const Node& l, const Node& r);

  inline const SequentCalculus Subtype{
    {Type},
    {Union, WhereOr},
    {Isect, WhereAnd},
    {WhereNot},
    {SubType},
    {
      TupleType >>
        [](const SequentCtx& ctx, Node& l, Node& r) {
          // Tuples must be the same arity and each element must be a subtype.
          return (l == TupleType) &&
            std::equal(
                   l->begin(),
                   l->end(),
                   r->begin(),
                   r->end(),
                   [&](auto& t, auto& u) { return Subtype(ctx, t, u); });
        },

      TypeName >>
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
            // Expand the alias: replace the alias reference with its body
            // and recurse. If both sides are aliases, expand left first;
            // recursion handles the right.
            // Note: assumes alias chains are acyclic (guaranteed by the ident
            // pass). Cyclic aliases would cause infinite recursion.
            if (l_def == TypeAlias)
            {
              auto new_ctx = build_typearg_ctx(ctx, l);
              return Subtype(new_ctx, l_def / Type, r);
            }
            else
            {
              auto new_ctx = build_typearg_ctx(ctx, r);
              return Subtype(new_ctx, l, r_def / Type);
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
        },

      Dyn >> AxiomTrue,
      None >> AxiomEq,
      Bool >> AxiomEq,
      I8 >> AxiomEq,
      I16 >> AxiomEq,
      I32 >> AxiomEq,
      I64 >> AxiomEq,
      U8 >> AxiomEq,
      U16 >> AxiomEq,
      U32 >> AxiomEq,
      U64 >> AxiomEq,
      ISize >> AxiomEq,
      USize >> AxiomEq,
      ILong >> AxiomEq,
      ULong >> AxiomEq,
      F32 >> AxiomEq,
      F64 >> AxiomEq,
      TypeSelf >> AxiomEq,
    },
    {
      // TypeSelf is always bound through implications (TypeSelf <: T and
      // T <: TypeSelf). It should never trigger contradiction detection
      // because the implications may not yet be decomposed when the atom
      // is checked.
      TypeSelf >>
        [](const SequentCtx&, Node&, Node&) { return false; },

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
