#include "subtype.h"

#include "bounds.h"

namespace vc
{
  struct Sequent
  {
    // TODO: self type, predicates, structural typing assumptions
    Nodes l_pending;
    Nodes r_pending;
    Nodes l_atomic;
    Nodes r_atomic;

    BoundsMap bounds;
    UnionFind uf;

    bool sub_reduce(Sequent&& that)
    {
      if (!that.reduce())
        return false;

      bounds.merge(that.bounds);
      uf.merge(that.uf);
      return true;
    }

    bool reduce(Node& l, Node& r)
    {
      // Start a fresh reduction.
      // TODO: keep the existing self type, predicates, and assumptions.
      Sequent seq;
      seq.l_pending.push_back(l);
      seq.r_pending.push_back(r);
      return sub_reduce(std::move(seq));
    }

    bool l_reduce(Node& t)
    {
      Sequent seq(*this);
      seq.l_pending.push_back(t);
      return sub_reduce(std::move(seq));
    }

    bool r_reduce(Node& t)
    {
      Sequent seq(*this);
      seq.r_pending.push_back(t);
      return sub_reduce(std::move(seq));
    }

    bool reduce()
    {
      while (!r_pending.empty())
      {
        auto r = r_pending.back();
        r_pending.pop_back();

        if (r == Union)
        {
          // Π ⊩ Γ ⊢ Δ, A, B
          // ---
          // Π ⊩ Γ ⊢ Δ, (A | B)

          // RHS union becomes RHS formulae.
          for (auto& t : *r)
            r_pending.push_back(t);
        }
        else if (r == Isect)
        {
          // Π ⊩ Γ ⊢ Δ, A
          // Π ⊩ Γ ⊢ Δ, B
          // ---
          // Π ⊩ Γ ⊢ Δ, (A & B)

          // RHS isect is a sequent split.
          for (auto& t : *r)
          {
            if (!r_reduce(t))
              return false;
          }

          return true;
        }
        else
        {
          r_atomic.push_back(r);
        }
      }

      while (!l_pending.empty())
      {
        auto l = l_pending.back();
        l_pending.pop_back();

        if (l == Isect)
        {
          // Π ⊩ Γ, A, B ⊢ Δ
          // ---
          // Π ⊩ Γ, (A & B) ⊢ Δ

          // LHS isect becomes LHS formulae.
          for (auto& t : *l)
            l_pending.push_back(t);
        }
        else if (l == Union)
        {
          // Π ⊩ Γ, A ⊢ Δ
          // Π ⊩ Γ, B ⊢ Δ
          // ---
          // Π ⊩ Γ, (A | B) ⊢ Δ

          // LHS union is a sequent split.
          for (auto& t : *l)
          {
            if (!l_reduce(t))
              return false;
          }

          return true;
        }
        else
        {
          l_atomic.push_back(l);
        }
      }

      // If either side is empty, the sequent is trivially false.
      if (l_atomic.empty() || r_atomic.empty())
        return false;

      // Π ⊩ Γ, A |- Δ, A
      // If any element in LHS is a subtype of any element in RHS, succeed.
      if (any_lr([&](Node& l, Node& r) { return subtype_atomic(l, r); }))
        return true;

      // If a type variable can be bound to make LHS <: RHS, do so and succeed.
      return any_lr([&](Node& l, Node& r) { return bind_typevar(l, r); });
    }

    template<typename F>
    bool any_lr(F&& f)
    {
      return std::any_of(l_atomic.begin(), l_atomic.end(), [&](Node& l) {
        return std::any_of(
          r_atomic.begin(), r_atomic.end(), [&](Node& r) { return f(l, r); });
      });
    }

    bool subtype_atomic(Node& l, Node& r)
    {
      // The atomic types are tuples, ref types, type names, and primitives.
      // Tuples must be the same arity and each element must be a subtype.
      if (r == TupleType)
      {
        return (l == TupleType) &&
          std::equal(
                 l->begin(),
                 l->end(),
                 r->begin(),
                 r->end(),
                 [&](auto& t, auto& u) { return reduce(t, u); });
      }
      else if (r == TypeNameReified)
      {
        // Reified type names are subtypes if they're identical.
        return r->equals(l);
      }
      else if (r->in(
                 {Dyn,
                  None,
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
                  F64}))
      {
        // Dynamic and primitives are subtypes if they're identical.
        return r->equals(l);
      }
      else if ((r == TypeVar) || (l == TypeVar))
      {
        // Don't infer type variable bindings here.
        return r->equals(l);
      }

      assert(false);
      return false;
    }

    bool bind_typevar(Node& l, Node& r)
    {
      if (l == TypeVar)
      {
        if (r == TypeVar)
        {
          // TODO: is this right, or should r be an upper bounds of l?
          uf.unite(l, r);
          return true;
        }

        // r is upper bounds for l
        bounds[l].use(r);
        return true;
      }
      else if (r == TypeVar)
      {
        // l is lower bounds for r
        bounds[r].assign(l);
        return true;
      }

      return false;
    }
  };

  bool subtype(const Node& l, const Node& r)
  {
    assert(l == Type);
    assert(r == Type);

    Sequent seq;
    seq.l_pending.push_back(l->front());
    seq.r_pending.push_back(r->front());
    return seq.reduce();
  }
}
