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
        [](Node& l, Node& r) {
          // Tuples must be the same arity and each element must be a subtype.
          return (l == TupleType) &&
            std::equal(
                   l->begin(),
                   l->end(),
                   r->begin(),
                   r->end(),
                   [](auto& t, auto& u) { return Subtype(t, u); });
        },

      // TODO: structural subtyping
      TypeName >> AxiomEq,
      Dyn >> AxiomEq,
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
      // TODO: contradiction axioms
    }};
}
