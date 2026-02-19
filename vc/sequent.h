#pragma once

#define TRIESTE_EXPOSE_LOG_MACRO
#include <trieste/trieste.h>

namespace trieste
{
  /**
   * @class SequentCalculus
   * @brief Implements a sequent calculus proof system for type checking and
   * logical reasoning.
   *
   * This class provides a framework for performing sequent calculus reductions,
   * which can be used to determine subtype relationships or logical
   * entailments. The sequent calculus operates on sequents of the form Γ ⊢ Δ,
   * where Γ (gamma) represents assumptions on the left-hand side and Δ (delta)
   * represents conclusions on the right-hand side.
   *
   * The class supports configurable logical connectives:
   * - **unwrap**: Tokens representing wrapper nodes that should be expanded
   * transparently.
   * - **or_**: Disjunction tokens (∨) - on RHS succeeds if any disjunct is
   * provable, on LHS causes a sequent split requiring all branches to succeed.
   * - **and_**: Conjunction tokens (∧) - on LHS expands conjuncts, on RHS
   * causes a sequent split requiring all conjuncts to be provable.
   * - **not_**: Negation tokens (¬) - moves the negated formula to the opposite
   * side. These must have a single child.
   * - **implies**: Implication tokens (→) - splits into antecedent on LHS and
   * consequent on RHS. These must have exactly two children.
   *
   * Axioms can be registered to define base-case proof rules for specific token
   * types, determining when atomic formulas on the LHS prove atomic formulas on
   * the RHS. An axiom returns true if the LHS formula proves the RHS formula.
   *
   * Contradiction axioms can optionally be registered separately from proof
   * axioms. These are used when checking for contradictions in the LHS
   * (uninhabited assumptions). Contradiction axioms may be weaker than proof
   * axioms, allowing types to be considered compatible for contradiction
   * detection even when they cannot prove each other. If no contradiction axiom
   * is specified for a token type, the proof axiom is used as a fallback. A
   * contradiction axiom must be symmetric: if A contradicts B, then B must
   * contradict A.
   *
   * @note The reduction algorithm detects contradictions in the LHS
   * (uninhabitable types) to prove any conclusion (ex falso quodlibet).
   *
   * @see Axiom for the function type used to define custom proof rules
   * @see AxiomEq for a default axiom that uses structural equality
   * @see AxiomTrue for a default axiom that considers all formulas provable
   */
  struct SequentCtx
  {
    Node scope;
    Nodes implies;
  };

  using Axiom = std::function<bool(const SequentCtx& ctx, Node& l, Node& r)>;
  inline const Axiom AxiomEq = [](const SequentCtx&, Node& l, Node& r) {
    return l->equals(r);
  };
  inline const Axiom AxiomTrue = [](const SequentCtx&, Node&, Node&) {
    return true;
  };

  struct SequentCalculus
  {
  private:
    std::vector<Token> unwrap;
    std::vector<Token> or_;
    std::vector<Token> and_;
    std::vector<Token> not_;
    std::vector<Token> implies;
    std::map<Token, Axiom> axioms;
    std::map<Token, Axiom> contradiction_axioms;

    struct State
    {
      SequentCtx ctx;
      Nodes lhs_pending;
      Nodes rhs_pending;
      Nodes lhs_atomic;
      Nodes rhs_atomic;
    };

  public:
    SequentCalculus(
      const std::initializer_list<Token>& unwrap,
      const std::initializer_list<Token>& or_,
      const std::initializer_list<Token>& and_,
      const std::initializer_list<Token>& not_,
      const std::initializer_list<Token>& implies,
      const std::initializer_list<std::pair<Token, Axiom>>& a,
      const std::initializer_list<std::pair<Token, Axiom>>& c = {})
    : unwrap(unwrap), or_(or_), and_(and_), not_(not_), implies(implies)
    {
      for (auto& axiom : a)
        axioms[axiom.first] = axiom.second;

      for (auto& axiom : c)
        contradiction_axioms[axiom.first] = axiom.second;
    }

    bool operator()(const SequentCtx& ctx, const Node& l, const Node& r) const
    {
      State state;
      state.ctx.scope = ctx.scope;
      state.lhs_pending = ctx.implies;
      state.lhs_pending.push_back(l);
      state.rhs_pending.push_back(r);
      auto res = reduce(state);

      LOG(Debug) << "Sequent = " << (res ? "true" : "false") << std::endl
                 << l << r << std::endl;

      return res;
    }

    bool operator()(const Node& scope, const Node& l, const Node& r) const
    {
      return (*this)(SequentCtx{scope, {}}, l, r);
    }

    bool invariant(const SequentCtx& ctx, const Node& l, const Node& r) const
    {
      return (*this)(ctx, l, r) && (*this)(ctx, r, l);
    }

    bool invariant(const Node& scope, const Node& l, const Node& r) const
    {
      return (*this)(SequentCtx{scope, {}}, l, r) &&
        (*this)(SequentCtx{scope, {}}, r, l);
    }

  private:
    bool reduce(State& state) const
    {
      while (!state.rhs_pending.empty())
      {
        auto r = state.rhs_pending.back();
        state.rhs_pending.pop_back();

        if (r->type().in(unwrap))
        {
          // Ignore this wrapper node.
          for (auto& t : *r)
            state.rhs_pending.push_back(t);
        }
        else if (r->type().in(or_))
        {
          // Π ⊩ Γ ⊢ Δ, A, B
          // ---
          // Π ⊩ Γ ⊢ Δ, A ∨ B

          // RHS 'or' succeeds if any are true.
          for (auto& t : *r)
            state.rhs_pending.push_back(t);
        }
        else if (r->type().in(and_))
        {
          // Π ⊩ Γ ⊢ Δ, A
          // Π ⊩ Γ ⊢ Δ, B
          // ---
          // Π ⊩ Γ ⊢ Δ, A ∧ B

          // RHS 'and' succeeds if all are true.
          for (auto& t : *r)
          {
            if (!split_right(state, t))
              return false;
          }

          return true;
        }
        else if (r->type().in(not_))
        {
          // Π ⊩ Γ, A ⊢ Δ
          // ---
          // Π ⊩ Γ ⊢ Δ, ¬A

          // RHS 'not' moves to LHS.
          assert(r->size() == 1);
          state.lhs_pending.push_back(r->front());
        }
        else if (r->type().in(implies))
        {
          // Π ⊩ Γ, A ⊢ Δ, B
          // ---
          // Π ⊩ Γ ⊢ Δ, A → B

          // RHS 'implies' splits to LHS and RHS.
          assert(r->size() == 2);
          state.lhs_pending.push_back(r->front());
          state.rhs_pending.push_back(r->back());
        }
        else
        {
          state.rhs_atomic.push_back(r);
        }
      }

      while (!state.lhs_pending.empty())
      {
        auto l = state.lhs_pending.back();
        state.lhs_pending.pop_back();

        if (l->type().in(unwrap))
        {
          // Ignore this wrapper node.
          for (auto& t : *l)
            state.lhs_pending.push_back(t);
        }
        else if (l->type().in(and_))
        {
          // Π ⊩ Γ, A, B ⊢ Δ
          // ---
          // Π ⊩ Γ, A ∧ B ⊢ Δ

          // LHS 'and' expands the conjuncts.
          for (auto& t : *l)
            state.lhs_pending.push_back(t);
        }
        else if (l->type().in(or_))
        {
          // Π ⊩ Γ, A ⊢ Δ
          // Π ⊩ Γ, B ⊢ Δ
          // ---
          // Π ⊩ Γ, A ∨ B ⊢ Δ

          // LHS 'or' is a sequent split.
          for (auto& t : *l)
          {
            if (!split_left(state, t))
              return false;
          }

          return true;
        }
        else if (l->type().in(not_))
        {
          // Π ⊩ Γ ⊢ Δ, A
          // ---
          // Π ⊩ Γ, ¬A ⊢ Δ

          // LHS 'not' moves to RHS.
          assert(l->size() == 1);
          return split_right(state, l->front());
        }
        else if (l->type().in(implies))
        {
          // Π, A → B ⊩ Γ ⊢ Δ, A
          // Π, A → B ⊩ Γ, B ⊢ Δ
          // ---
          // Π ⊩ Γ, A → B ⊢ Δ

          // LHS 'implies' is a sequent split. Keep the implication in the
          // context for both branches, since it can be used as an assumption in
          // either branch.
          state.ctx.implies.push_back(l);
          assert(l->size() == 2);
          return split_right(state, l->front()) && split_left(state, l->back());
        }
        else
        {
          // Check for contradictions.
          for (auto& t : state.lhs_atomic)
          {
            if (contradiction(state, t, l))
              return true;
          }

          state.lhs_atomic.push_back(l);
        }
      }

      // Π ⊩ Γ, A ⊢ Δ, A
      // If any element in LHS proves any element in RHS, succeed.
      return std::any_of(
        state.lhs_atomic.begin(), state.lhs_atomic.end(), [&](Node& l) {
          return std::any_of(
            state.rhs_atomic.begin(), state.rhs_atomic.end(), [&](Node& r) {
              auto find = axioms.find(r->type());

              if (find != axioms.end())
                return find->second(state.ctx, l, r);

              return false;
            });
        });
    }

    bool split_left(State& state, const Node& node) const
    {
      State split = state;
      split.lhs_pending.push_back(node);
      return reduce(split);
    }

    bool split_right(State& state, const Node& node) const
    {
      State split = state;
      split.rhs_pending.push_back(node);
      return reduce(split);
    }

    bool contradiction(const State& state, Node& l, Node& r) const
    {
      // Check contradiction axioms for both sides. Contradiction is
      // symmetric: if either side's axiom says "not contradictory", honor
      // that.
      auto cr = contradiction_axioms.find(r->type());
      auto cl = contradiction_axioms.find(l->type());
      bool has_axiom =
        (cr != contradiction_axioms.end()) ||
        (cl != contradiction_axioms.end());

      if ((cr != contradiction_axioms.end()) && !cr->second(state.ctx, l, r))
        return false;

      if ((cl != contradiction_axioms.end()) && !cl->second(state.ctx, r, l))
        return false;

      if (has_axiom)
        return true;

      // By default, if L does not prove R and R does not prove L, then they
      // contradict.
      auto r_find = axioms.find(r->type());
      if ((r_find != axioms.end()) && (r_find->second(state.ctx, l, r)))
        return false;

      auto l_find = axioms.find(l->type());
      if ((l_find != axioms.end()) && (l_find->second(state.ctx, r, l)))
        return false;

      return true;
    }
  };

  inline std::pair<Token, Axiom> operator>>(Token token, Axiom apply)
  {
    return {token, apply};
  }
}
