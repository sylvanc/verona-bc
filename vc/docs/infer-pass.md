# Infer Pass: Design

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

```
build_branch_exits(env, label):
  if label.terminator is Cond with typetest trace:
    narrowed = trace_typetest(terminator)
    true_exit = clone(env); true_exit[narrowed.src] = narrowed.type
    false_exit = env
    store branch_exit(label, true_target) = true_exit
    store branch_exit(label, false_target) = false_exit
```

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
  // Set lambda params from cown types
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
`call_node` (tracking which Call/CallDyn produced it):

- **Call**: re-infer TypeArgs with concrete type, re-constrain args
- **CallDyn**: re-resolve Lookup, refine callee's return path

### call_node Set When

- Call's TypeArgs inferred entirely from default-typed args
- CallDyn's result contains DefaultInt/DefaultFloat

Propagated through Copy/Move.

### propagate_call_node

```
propagate_call_node(env, loc):
  if env[loc] changed from DefaultInt to concrete and env[loc].call_node:
    backward_refine_call(env[loc].call_node, concrete_type)
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

The deferred loop handles TypeVar params/returns that depend on
caller-side propagation. The intra-function fixpoint handles everything
else, including DefaultInt refinement via call_node.

## 9. Finalization

After the fixpoint converges:

1. **Consts**: write inferred types to AST nodes
2. **Return type**: collect from Return terminators across all labels,
   build Union if multiple. In generic contexts, if any CallDyn dst
   is missing from env, leave as TypeVar.
3. **Params and fields**: update ParamDef/FieldDef with inferred types

## 10. What This Eliminates

| Old feature | Replaced by |
|-------------|------------|
| Lambda grafting | call_node backward propagation |
| run_cascade (reverse index + worklist) | Fixpoint re-processes statements |
| Phase A / A.5 / B distinction | Single unified fixpoint |
| apply_branch_narrowing | Cond exit envs + normal merge |
| Separate resolve/expect/refine | Unified transfer functions with merge |
| Shared vs per-label env modes | Per-label everywhere |
| Lambda Const finalization | Each lambda finalizes its own |
| has_typevar body-Const check | Not needed |

## 11. Size Estimate

| Component | Lines |
|-----------|-------|
| Data structures + helpers | ~100 |
| Dispatch tables | ~60 |
| Transfer functions (all stmts) | ~300 |
| merge_type | ~80 |
| TypeArg inference | ~200 |
| call_node backward propagation | ~80 |
| process_function + convergence | ~60 |
| merge_predecessors + branch exits | ~50 |
| Finalize + return inference | ~100 |
| Deferred loop + PassDef | ~50 |
| **Total** | **~1080** |

## 12. Case Verification

| Case | Forward (resolve) | Backward (expect) | Result |
|------|-------------------|-------------------|--------|
| Literal + captured field | FieldRef → i32, arith → i32 | Arith merges i32 into RHS | ✓ |
| Literal in lambda return | Union(nomatch, DefaultInt) | Copy merges i32 → call_node | ✓ |
| Generic when + match | TypeVar (unresolved CallDyn) | No concrete merge | Reify ✓ |
| Generic when + literal | Method unresolvable | No merge | Annotation ✓ |
| Generic Call defaults | wrapper[DefaultInt] | Copy merges → call_node | ✓ |
| Typetest narrowing | Cond true exit = narrowed | Normal merge at join | ✓ |

## 13. Properties

- **Monotonicity**: merges move up the lattice, never down
- **Convergence**: finite lattice × monotonic = terminates
- **No phases**: one loop, one operation (merge), bidirectional per stmt
- **No grafting**: cross-lambda flow via call_node + fixpoint
- **No cascade**: fixpoint re-processes all statements
- **Principled**: standard bidirectional dataflow analysis on type lattice
