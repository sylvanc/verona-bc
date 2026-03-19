# Infer Pass: Design and Implementation Guide

## 1. Algorithm

Worklist-driven bidirectional dataflow analysis with separate forward
and backward environments per label. Forward env flows predecessors →
successors. Backward env flows successors → predecessors. Combined at
each label's entry to produce the working env for body processing.

## 2. Architecture (as implemented in infer.cc)

### Data Structures

```cpp
struct LocalTypeInfo {
  Node type;       // Source-level Type node
  bool is_fixed;   // True for params, TypeAssertion
  Node call_node;  // Call/CallDyn that produced this
};
using TypeEnv = std::map<Location, LocalTypeInfo>;
```

### merge_type — returns {} for no-change

Key properties:
- `merge(TypeVar, TypeVar) = {}` (both unknown, no change)
- `merge(TypeVar, X) = X` (TypeVar yields)
- `merge(X, TypeVar) = {}` (existing already has info)
- Default yielding checked BEFORE `Subtype.invariant` — because
  `AxiomFalse` makes `Subtype.invariant(DefaultInt, i32)` return
  true, which would block refinement
- Union default replacement: when building a Union with a concrete
  incoming primitive, compatible Default{Int,Float} members are removed
- Returns `{}` for no-change, enabling cheap pointer-based convergence

### merge_env

Creates new entries (returns true). Updates existing entries only when
`merge_type` returns non-null (returns true). Returns false otherwise.

### Worklist (process_function)

```
exit_envs[0] = params
bwd_envs[return_label] = {ret_loc: declared_return_type}
worklist = {all labels}

while worklist not empty:
  i = worklist.pop_front()  // std::set, ascending order
  
  entry = merge(predecessor exits, successor bwd_envs)
  env = copy(entry)
  
  label_changed = process_label_body(env, bwd_envs[i])
  transfer_terminator(env)  // Return backward, Cond narrowing
  
  exit_envs[i] = move(env)
  if label_changed: worklist += successors
  
  propagate bwd_envs from successors to self
  if bwd_changed: worklist += predecessors
```

### process_label_body — returns bool

Two merge lambdas:
- `merge(loc, type, call_node={})` — forward merge, sets `any_changed`
- `merge_bwd(loc, type)` — backward merge, sets `any_changed` AND
  records into `bwd_envs[i]` (only concrete non-union types)

## 3. Transfer Functions

All use `merge` for forward, `merge_bwd` for backward (before
`propagate_call_node` calls).

### Key patterns:
- **Const**: `merge(dst, literal_type)`
- **Copy/Move**: `merge(dst, env[src])` forward, `merge_bwd(src, env[dst])` backward
- **Call**: `infer_typeargs`, forward return, backward params into args, `push_arg_types_to_params`
- **Lookup**: propagate DefaultInt for default receivers, resolve for concrete
- **CallDyn**: forward from Lookup, backward from resolved method params (skip if receiver default), `push_arg_types_to_params`, `propagate_shape_to_lambda`
- **Return terminator**: `merge_env(ret_loc, declared_return_type)` + record in bwd_envs
- **Binary ops**: backward from dst into lhs/rhs
- **Store**: backward into stored value

## 4. Subtype Integration

TypeVar has `AxiomEq` + `AxiomFalse` in vc/subtype.h.

## 5. WF Details

- `TypeAssertion <<= LocalId * Type` (NOT Rhs)
- `NewArrayConst <<= wfDst * Type * (Rhs >>= wfIntLiteral)` — size via `stmt / Rhs`
- `ArrayRefConst <<= wfDst * Arg * (Rhs >>= wfIntLiteral)` — index via `stmt / Rhs`
- `Store <<= wfDst * wfSrc * Arg` — dst=LocalId, ref=Rhs, val=Arg
- `FieldRef <<= wfDst * Arg * FieldId`

## 6. Inter-Function (Deferred Loop)

Standard: first pass processes all functions. Deferred loop re-processes
functions with `has_typevar`. Final sweep: DefaultInt→u64, DefaultFloat→f64.

## 7. Current Status

- 2466 lines (down from 4279)
- 0.5s per test (hello)
- 50/72 exit codes correct (22 wrong)
- Baseline: 71/72 (1 pre-existing generic_when failure)
