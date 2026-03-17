# Infer Pass: Design and Implementation Guide

## Implementation Prompt

**Task**: Rewrite `vc/passes/infer.cc` according to this design document.
The old implementation is backed up at `vc/passes/infer_old.cc`.

**Approach**:
1. Keep ALL helper functions from the old file (lines 1-1883): constants,
   dispatch tables, type constructors, extract_*, resolve_*, apply_subst,
   merge_type, env_changed, trace_typetest, backward_refine_call/calldyn,
   infer_call (for TypeArg inference), navigate_call, propagate_shape_to_lambda.
2. DELETE: run_cascade, SrcIndex, lambda grafting block in CallDyn handler,
   lambda_cache, lambda Const finalization tree traversal, has_typevar
   body-Const check, enclosing_func parameter.
3. REWRITE process_label_body: Replace the 1179-line function with a
   ~300-line transfer function dispatch. Each statement handler does both
   forward AND backward via merge. No grafting. No phases.
4. REWRITE process_function: Replace the 839-line function with a ~100-line
   fixpoint loop over per-label environments. No Phase A/A.5/B. No cascade.
   No 8 post-convergence passes.
5. REWRITE infer() PassDef: Simplify the deferred loop. Remove body-Const
   heuristic from has_typevar.
6. Build, test with: `ninja install && ctest -R "compile-exit_code|run-exit_code"`
7. Test specifically: generic_when, match_value_infer, match_value_match,
   infer_backward, iowise (`./dist/vc/vc build ../../iowise/`)

**Key principle**: ALL env updates use `merge_type`. Never overwrite.
The fixpoint loop handles convergence automatically.

**Key change from old design**: The Copy/Move transfer function does
BACKWARD merge (`env[src] = merge(env[src], env[dst])`), which enables
type expectations to flow from assignment targets back to sources. This
is what replaces lambda grafting — when a DefaultInt flows through a
CallDyn return and gets copied to a typed destination, the backward
merge triggers call_node propagation back into the lambda.

**Critical detail for call_node**: The CallDyn handler must set
`call_node` on its result entry when the result type contains
DefaultInt/DefaultFloat (not just when the receiver is default, as
the old code did). This enables the Copy/Move backward merge to
trigger cross-lambda backward propagation.

**Critical detail for merge in Copy**: The old Copy handler just cloned
the source type. The new one does `merge(env[dst], env[src])` for
forward AND `merge(env[src], env[dst])` for backward. This means on
the SECOND fixpoint iteration, if dst was refined to i32 by a later
statement's backward merge, Copy's backward direction merges i32 into
src (which might be DefaultInt), refining it.

---

## 1. Algorithm in One Sentence

Repeatedly apply each statement's bidirectional transfer function to
per-label environments, merging at join points, until nothing changes.

## 2. Purpose

Determine concrete types for every local variable in every function:
- Refine integer/float literals (`DefaultInt` → `i32`, etc.)
- Resolve TypeVar params and returns
- Infer TypeArgs for generic calls

## 3. Foundations

### Type Lattice

Types form a lattice under `merge`:
- **Bottom**: empty (no type known yet)
- **Top**: Dyn
- **DefaultInt/DefaultFloat**: constraint variables that yield to
  compatible concrete primitives
- **TypeVar**: yields to anything
- **Concrete types**: primitives, TypeNames, Unions

`merge(A, B)` computes the least upper bound:

```
merge(⊥, X)           = X
merge(X, ⊥)           = X
merge(A, A)            = A               (idempotent)
merge(TypeVar, X)      = X               (TypeVar yields)
merge(DefaultInt, i32) = i32             (default yields to concrete)
merge(i32, u64)        = Union(i32, u64) (incompatible → union)
```

Subtype pruning: if `A <: B`, `merge(A, B) = B`.

### Monotonicity and Convergence

All env updates are merges. Types move up the lattice (more specific),
never down. The lattice has finite height. The fixpoint terminates.

### Bidirectional Transfer Functions

Each statement kind has a transfer function that reads from and writes
to the env in BOTH directions:

- **Forward**: compute output type from input types
- **Backward**: constrain input types from output/context types

Both use merge. The fixpoint automatically handles the interaction.

## 4. DefaultInt / DefaultFloat

Constraint variables: "some integer/float type, TBD."

- DefaultInt yields to any integer primitive (i8..u64, isize, usize)
- DefaultFloat yields to any float primitive (f32, f64)
- Neither yields to non-primitives

**Late resolution**: DefaultInt propagates freely through Copy/Move/Call/
CallDyn returns. Refined only when merged with a concrete type via a
transfer function's backward direction. Unrefined sentinels become
u64/f64 in the final sweep.

## 5. Intra-Function Fixpoint

### Per-Label Environments

- Each label has an **exit env** (result of processing its body)
- **Entry env** = merge of all predecessor exit envs
- Cond terminators produce **two exit envs** (true/false), each with
  the typetest variable narrowed to its tested type
- Start label's initial exit env = params

### Algorithm

```
process_function(f):
  exit_envs = {}
  exit_envs[start] = build_from_params(f)
  
  changed = true
  while changed:
    changed = false
    for each label:
      entry = merge_predecessors(exit_envs, label)
      env = clone(entry)
      
      for each stmt in label.body:
        transfer(env, stmt)
      
      build_branch_exits(env, label)  // Cond typetest → two exits
      
      if env changed from exit_envs[label]:
        exit_envs[label] = env
        changed = true
  
  finalize(exit_envs, f)
```

### merge_predecessors

```
merge_predecessors(exit_envs, label):
  if label is start: return exit_envs[start]
  entry = {}
  for pred in predecessors(label):
    pred_exit = branch_exit(pred, label) or exit_envs[pred]
    for (loc, type) in pred_exit:
      entry[loc] = merge(entry[loc], type)
  return entry
```

### build_branch_exits

For Cond terminators with typetest traces:
```
build_branch_exits(env, label):
  if label.terminator is Cond with typetest trace:
    narrowed = trace_typetest(terminator)
    true_exit = clone(env); true_exit[narrowed.src] = narrowed.type
    false_exit = env
    store branch_exit(label, true_target) = true_exit
    store branch_exit(label, false_target) = false_exit
```

For non-Cond terminators, no branch exits — successors use the
label's normal exit env.

## 6. Transfer Functions

Each statement kind has a single transfer function that performs both
forward and backward updates via merge.

### Copy / Move

```
transfer_copy(env, dst, src):
  env[dst] = merge(env[dst], env[src])     // forward: propagate
  env[src] = merge(env[src], env[dst])     // backward: refine src from dst
  propagate_call_node(env, src)            // cross-function if changed
```

### Const

```
transfer_const(env, dst, literal):
  type = default_literal_type(literal)     // DefaultInt, Bool, etc.
  env[dst] = merge(env[dst], type)
```

### Call

```
transfer_call(env, dst, func, args):
  // TypeArg inference: match formals against actuals
  infer_typeargs(env, func, args)
  
  // Forward: compute return type
  ret = apply_subst(func.return_type, subst)
  env[dst] = merge(env[dst], ret)
  
  // Backward: constrain args from param types
  for (arg, param) in zip(args, func.params):
    expected = apply_subst(param.type, subst)
    env[arg] = merge(env[arg], expected)
    propagate_call_node(env, arg)
```

### CallDyn / TryCallDyn

```
transfer_calldyn(env, dst, lookup_src, args):
  // Forward: result type from Lookup
  env[dst] = merge(env[dst], env[lookup_src])
  set_call_node_if_default(env[dst], stmt)
  
  // Backward: constrain args from resolved method params
  method = resolve_method(env, stmt)
  if method:
    for (arg, param) in zip(args, method.params):
      expected = apply_subst(param.type, method.subst)
      env[arg] = merge(env[arg], expected)
      propagate_call_node(env, arg)
```

### Lookup

```
transfer_lookup(env, dst, receiver, method_name, ...):
  ret = resolve_method_return(env, receiver, method_name, ...)
  if ret:
    env[dst] = merge(env[dst], ret)
```

### New / Stack

```
transfer_new(env, dst, type, args):
  env[dst] = merge(env[dst], type)
  // Backward: constrain args from field types
  for (arg, field) in zip(args, class_fields):
    env[arg] = merge(env[arg], field.type)
    propagate_call_node(env, arg)
```

### FieldRef / Load / Store

```
transfer_fieldref(env, dst, obj, field_name):
  field_type = resolve_field_type(env, obj, field_name)
  env[dst] = merge(env[dst], ref(field_type))

transfer_load(env, dst, src):
  inner = unwrap_ref(env[src])
  if inner: env[dst] = merge(env[dst], inner)

transfer_store(env, dst, ref_src, val_src):
  inner = unwrap_ref(env[ref_src])
  if inner:
    env[dst] = merge(env[dst], inner)
    env[val_src] = merge(env[val_src], inner)  // backward: refine stored value
    propagate_call_node(env, val_src)
```

### When

```
transfer_when(env, dst, lookup_src, args):
  apply_ret = env[lookup_src]
  env[dst] = merge(env[dst], cown(apply_ret))
  // Set lambda params from cown types (always, not just when TypeVar)
  set_when_lambda_params(env, args)
```

### Return

```
transfer_return(env, src, func):
  env[src] = merge(env[src], func.return_type)  // backward: refine from declared return
  propagate_call_node(env, src)
```

### Arithmetic / Unary / Fixed-Result

```
transfer_arithmetic(env, dst, lhs, rhs):
  env[dst] = merge(env[dst], env[lhs])          // forward: result = lhs type
  env[rhs] = merge(env[rhs], env[lhs])          // backward: refine rhs from lhs
  propagate_call_node(env, rhs)

transfer_fixed(env, dst, result_type):
  env[dst] = merge(env[dst], result_type)        // e.g., Bool, USize, None
```

## 7. Cross-Function Propagation: call_node

When a merge changes a DefaultInt to concrete, and the entry has
`call_node` (tracking which Call/CallDyn produced it), propagate the
refinement back into the callee.

Uses existing `backward_refine_call` and `backward_refine_calldyn`
functions from the old implementation.

### call_node Set When

- A Call's TypeArgs were inferred entirely from default-typed args
  (`all_default_inference` in `infer_call`)
- A CallDyn's result type contains DefaultInt/DefaultFloat
  (**NEW**: old code only set when RECEIVER was default)

Propagated through Copy/Move via `LocalTypeInfo::propagated`.

### propagate_call_node

```
propagate_call_node(env, loc):
  if env[loc] changed from DefaultInt to concrete and env[loc].call_node:
    if call_node is Call:
      backward_refine_call(call_node, concrete_type, env, top)
    else if call_node is CallDyn:
      backward_refine_calldyn(call_node, concrete_prim, env, top, lookup_stmts)
```

## 8. Inter-Function Fixpoint (Deferred Loop)

```
infer():
  post(top):
    deferred = []
    for each Function f in top:
      process_function(f)
      if has_typevar(f): deferred.push(f)
    
    while progress:
      for f in deferred:
        reset_return_to_typevar(f)
        process_function(f)
      progress = typevar_count decreased
    
    sweep: DefaultInt → u64, DefaultFloat → f64
```

### has_typevar (simplified)

Returns true if:
- Any param has TypeVar type
- Return type is TypeVar, DefaultInt, or DefaultFloat
- Return is Union containing DefaultInt/DefaultFloat

Does NOT check body Consts (old grafting heuristic removed).

## 9. Finalization

After the intra-function fixpoint converges, for each label:

1. **Consts**: Write inferred types to AST nodes. For each Const in the
   exit env, extract the primitive token and update the Const's type
   position. Keep DefaultInt/DefaultFloat if the env still has default
   (for cross-function propagation).

2. **TypeAssertion**: Remove from body.

3. **NewArrayConst/tuple**: Update Type child from exit env.

Then for the function:

4. **Return type**: Collect from Return terminators across all labels.
   In generic contexts, if any CallDyn/TryCallDyn dst is missing from
   exit env, leave as TypeVar. Otherwise build Union with Subtype pruning.

5. **TypeVar back-prop**: Fixpoint over typevar_aliases (Copy/Move edges
   where one side was TypeVar).

6. **Params and fields**: Update ParamDef/FieldDef AST with inferred types.

## 10. What This Eliminates

| Old feature | Replaced by |
|-------------|------------|
| Lambda grafting (~120 lines) | call_node + Copy/Move backward merge |
| run_cascade (~260 lines) | Fixpoint re-processes statements |
| Phase A / A.5 / B | Single unified fixpoint |
| apply_branch_narrowing | Cond exit envs + normal merge |
| Separate resolve/expect/refine | Unified transfer functions with merge |
| Shared vs per-label env | Per-label everywhere |
| Lambda Const finalization (~30 lines) | Each lambda finalizes its own |
| has_typevar body-Const check | Not needed |
| SrcIndex reverse map | Not needed |
| enclosing_func parameter | Not needed |

## 11. Helper Functions to Keep from Old Implementation

These functions implement language semantics and are reused as-is:

**Type constructors**: primitive_type, ffi_primitive_type, string_type,
ref_type, cown_type

**Type extraction**: extract_wrapper_inner, extract_ref_inner,
extract_cown_inner, extract_primitive, extract_callable_primitive,
extract_wrapper_primitive, direct_typeparam

**Type operations**: is_default_type, resolve_default, build_class_subst,
extract_constraints, apply_subst, contains_typevar, merge_type

**Method resolution**: resolve_method, resolve_method_return_type,
navigate_call, propagate_shape_to_lambda

**Refinement**: default_literal_type, refine_const, try_refine

**Backward propagation**: backward_refine_call, backward_refine_calldyn

**TypeArg inference**: infer_call (keep the TypeArg + Phase 3/4 logic;
it's called from the Call transfer function)

**Typetest**: trace_typetest

**Convergence**: env_changed (used to check if exit env changed)

**Data structures**: LocalTypeInfo, TypeEnv, MethodInfo, ScopeInfo,
TypetestTrace, dispatch tables

## 12. Size Estimate

| Component | Lines |
|-----------|-------|
| Kept helpers (from old impl) | ~900 |
| Dispatch tables | ~60 |
| Transfer function dispatch | ~300 |
| merge_type (kept) | ~80 |
| process_function + convergence | ~80 |
| merge_predecessors + branch exits | ~50 |
| Finalize + return inference | ~120 |
| Deferred loop + PassDef | ~50 |
| **Total** | **~1640** |

(Higher than theoretical ~1080 because kept helpers include some that
could be simplified but are preserved for safety in the initial rewrite.)

## 13. Case Verification

| Case | Forward | Backward | Result |
|------|---------|----------|--------|
| Literal + captured field | FieldRef → i32, arith → i32 | Arith merges i32 into RHS | ✓ |
| Literal in lambda return | Union(nomatch, DefaultInt) | Copy merges i32 → call_node | ✓ |
| Generic when + match | TypeVar (unresolved CallDyn) | No concrete merge | Reify ✓ |
| Generic when + literal | Method unresolvable | No merge | Annotation ✓ |
| Generic Call defaults | wrapper[DefaultInt] | Copy merges → call_node | ✓ |
| Typetest narrowing | Cond true exit = narrowed | Normal merge at join | ✓ |

## 14. Properties

- **Monotonicity**: merges move up the lattice, never down
- **Convergence**: finite lattice × monotonic = terminates
- **No phases**: one loop, one operation (merge), bidirectional per stmt
- **No grafting**: cross-lambda flow via call_node + fixpoint
- **No cascade**: fixpoint re-processes all statements
- **Principled**: standard bidirectional dataflow analysis on type lattice
