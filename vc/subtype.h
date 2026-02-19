#pragma once

#include "bounds.h"
#include "lang.h"
#include "sequent.h"

namespace vc
{
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

          if ((l_def == TypeAlias) || (r_def == TypeAlias))
          {
            // Build SubType implications from a TypeName reference's path.
            // Each TypeArg on a NameElement is paired with the corresponding
            // TypeParam of that scope, creating bidirectional SubType
            // implications to express equality (TypeParam = TypeArg).
            auto build_ctx =
              [&](const SequentCtx& base, const Node& name) -> SequentCtx {
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

                while (
                  ta_it != type_args->end() &&
                  tp_it != type_params->end())
                {
                  if (*tp_it == TypeParam)
                  {
                    // Build a FQ TypeName for this TypeParam.
                    auto make_fq_tp = [&]() {
                      Node fq_tp = TypeName;
                      for (size_t j = 0; j <= i; j++)
                      {
                        fq_tp
                          << (NameElement
                              << clone(name->at(j) / Ident) << TypeArgs);
                      }
                      fq_tp
                        << (NameElement
                            << clone((*tp_it) / Ident) << TypeArgs);
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
            };

            // Expand the alias: replace the alias reference with its body
            // and recurse. If both sides are aliases, expand left first;
            // recursion handles the right.
            // Note: assumes alias chains are acyclic (guaranteed by the ident
            // pass). Cyclic aliases would cause infinite recursion.
            if (l_def == TypeAlias)
            {
              auto new_ctx = build_ctx(ctx, l);
              return Subtype(new_ctx, l_def / Type, r);
            }
            else
            {
              auto new_ctx = build_ctx(ctx, r);
              return Subtype(new_ctx, l, r_def / Type);
            }
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
    },
    {
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

          // r is a concrete ClassDef.
          if (l != TypeName)
          {
            // l is a non-TypeName atom (primitive, tuple, etc.).
            // Different nominal types contradict.
            return true;
          }

          auto l_def = find_def(ctx.scope, l);
          if (!l_def || l_def->type().in({TypeParam, TypeAlias}))
            return false;

          // Both are concrete ClassDefs. Different defs contradict.
          return l_def != r_def;
        },
    }};
}
