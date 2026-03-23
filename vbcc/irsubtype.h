#pragma once

#include "sequent.h"
#include "vbcc.h"

namespace vbcc
{
  using namespace trieste;

  inline const Axiom IRAxiomEq(const SequentCalculus& c)
  {
    return [&](const SequentCtx& ctx, Node& l, Node& r) {
      if (l == TypeId)
      {
        for (auto& child : *ctx.scope)
        {
          if ((child == Type) && (child / TypeId)->equals(l))
            return c(ctx, child / Type, r);
        }
      }

      return l->equals(r);
    };
  }

  inline const Axiom IRAxiomInvariant(const SequentCalculus& c)
  {
    return [&](const SequentCtx& ctx, Node& l, Node& r) {
      if (l == TypeId)
      {
        for (auto& child : *ctx.scope)
        {
          if ((child == Type) && (child / TypeId)->equals(l))
            return c(ctx, child / Type, r);
        }
      }

      return (l->type() == r->type()) && c.invariant(ctx, l / Type, r / Type);
    };
  }

  inline const SequentCalculus IRSubtype = {
    {},
    {Union},
    {},
    {},
    {},
    {
      Dyn >> AxiomTrue,
      None >> IRAxiomEq(IRSubtype),
      Bool >> IRAxiomEq(IRSubtype),
      I8 >> IRAxiomEq(IRSubtype),
      I16 >> IRAxiomEq(IRSubtype),
      I32 >> IRAxiomEq(IRSubtype),
      I64 >> IRAxiomEq(IRSubtype),
      U8 >> IRAxiomEq(IRSubtype),
      U16 >> IRAxiomEq(IRSubtype),
      U32 >> IRAxiomEq(IRSubtype),
      U64 >> IRAxiomEq(IRSubtype),
      ISize >> IRAxiomEq(IRSubtype),
      USize >> IRAxiomEq(IRSubtype),
      ILong >> IRAxiomEq(IRSubtype),
      ULong >> IRAxiomEq(IRSubtype),
      F32 >> IRAxiomEq(IRSubtype),
      F64 >> IRAxiomEq(IRSubtype),
      Ptr >> IRAxiomEq(IRSubtype),
      ClassId >> IRAxiomEq(IRSubtype),
      Array >> IRAxiomInvariant(IRSubtype),
      Ref >> IRAxiomInvariant(IRSubtype),
      Cown >> IRAxiomInvariant(IRSubtype),

      TupleType >>
        [](const SequentCtx& ctx, Node& l, Node& r) {
          if (l == TypeId)
          {
            for (auto& child : *ctx.scope)
            {
              if ((child == Type) && (child / TypeId)->equals(l))
                return IRSubtype(ctx, child / Type, r);
            }
          }

          return (l == TupleType) && (r == TupleType) &&
            std::equal(
                   l->begin(),
                   l->end(),
                   r->begin(),
                   r->end(),
                   [&](auto& t, auto& u) { return IRSubtype(ctx, t, u); });
        },

      TypeId >>
        [](const SequentCtx& ctx, Node& l, Node& r) {
          if (l->equals(r))
            return true;

          for (auto& child : *ctx.scope)
          {
            if ((child == Type) && (child / TypeId)->equals(r))
              return IRSubtype(ctx, l, child / Type);
          }

          return false;
        },
    },
    {}};
}
