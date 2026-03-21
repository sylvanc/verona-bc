---
name: verona-type-inference
description: Verona compiler type inference model — bidirectional refinement, per-function inference, lambda/context propagation, algebraic and structural typing, generics, and debugging. Use when changing or debugging `vc/passes/infer.cc` or investigating type inference failures.
user-invocable: false
---

# Verona Type Inference

Use this skill when working on `vc/passes/infer.cc`, investigating compile-time
type errors rooted in inference, or debugging runtime `bad type` failures caused
by imprecise compiler output.

## Mental Model

- Verona type inference is **bidirectional refinement**, not Hindley-Milner
  style global unification.
- Information must flow in **both directions**:
  - **forward** from definitions, annotations, receivers, fields, and callee
    signatures to uses
  - **backward** from uses, expected result types, stores, and later call sites
    back to the expressions that produced a value
- Inference is primarily **local per function**. The main exception is that
  inference must flow **into and out of lambdas** within that function.
- Think in terms of **checking under context**, not just synthesizing from local
  syntax. Expected types are part of the inference problem, especially for
  lambdas, default literals, and generic calls.
- `any` is a **top type**.
- At Verona source inference level, there is **no bottom type**. Do not treat
  `nomatch`, an empty-looking union, failed shape resolution, or any other
  artifact as a source-level bottom during inference. Even if later IR or
  subtype machinery has a bottom-like encoding, that is not a license to inject
  bottom into source inference.

## Literature Anchors

Verona inference is not novel; it is best understood as a combination of known
ideas:

- **Bidirectional typing** (Pierce & Turner; Dunfield & Krishnaswami): use
  expected types to check introduction forms like lambdas, and synthesize types
  for elimination forms like calls and lookups.
- **Local type inference** (Pierce & Turner): infer from nearby syntax,
  annotations, and use sites instead of trying to recover a single global
  principal typing.
- **Flow-sensitive / occurrence typing** (Tobin-Hochstadt & Felleisen style):
  typetests refine branch-local environments, and joins must preserve soundness.
- **Algebraic / semantic subtyping** (Frisch, Castagna, Benzaken lineage):
  unions and intersections are semantic type constructors; subtyping and
  normalization matter more than raw tree equality.
- **Constraint-based generic inference**: infer type arguments from actual
  arguments, receivers, fields, and expected results. With subtyping,
  intersections, and shapes, principal types often do not exist, so rely on
  local obligations rather than global search.

## Verona-Specific Invariants

- The infer pass runs on **ANF**, so reason in terms of `LocalId`s, labels, and
  statement-local definitions rather than high-level source syntax.
- The engine is organized around a **per-function fixpoint** over label entry
  and exit environments.
- Use `exit_envs`, not just the merged global environment, when precision
  matters. Branch-local narrowing often exists only in `exit_envs`.
- The infer pass is not just about compile-time error messages. A missed
  refinement can compile and then fail later at runtime with `bad type`.
- Prefer **subtyping-aware reasoning** over syntactic equality. Equality checks
  are only safe where the representation is already canonical and that is the
  intended invariant.

## Key Compiler Hooks

- `process_function()` drives the per-function fixpoint.
- `TypeEnv` maps `Location -> LocalTypeInfo`.
- `merge()` controls how environments join and how default numeric types are
  promoted or preserved.
- `trace_typetest()` drives branch-local narrowing.
- `infer_call()` propagates forward information into calls.
- `backward_refine_call()` and `backward_refine_calldyn()` propagate expected
  information backward into earlier calls and dynamic lookups.
- `propagate_shape_to_lambda()` is where callable/shape context is pushed into a
  lambda's `apply`.
- Return inference in `process_function()` must read from `exit_envs`, because
  typetest narrowing can make return types more precise than the global env.

## Propagation Directions

### Forward propagation

- Parameter types into locals and bodies
- Receiver and method signature information into call arguments
- Explicit annotations into assigned or returned expressions
- Field types into `new` arguments and stores
- Constructor/member structure into subexpressions
- Typetest results into branch-local envs

### Backward propagation

- Expected result types back into prior calls
- Typed stores back into the values being stored
- Typed vars and returns back into producers
- Expected callable types back into lambda params and lambda result obligations
- Later concrete uses back into earlier default-typed numeric literals

When diagnosing a bug, first ask **which direction is missing**. Many failures
look like "inference forgot X" when the real issue is that the needed backward
edge or lambda-context edge was never modeled.

## Lambdas Are Part of the Local Inference Problem

- Lambdas are the main exception to "per-function local" inference: they are not
  isolated from their enclosing function.
- Inference must flow **into** a lambda from an expected callable or shape type,
  and **out of** the lambda body via synthesized information and captured uses.
- Treat lambda typing as a **bidirectional checking** problem:
  - the expected callable type provides parameter expectations and a result
    obligation
  - the lambda body still synthesizes information that must be propagated back
    into the enclosing function's local problem
- For a lambda checked against `(P1, ..., Pn) -> R`, the right rule is:
  parameter types come from the context, and the body must check against `R`.
  If the body synthesizes `S`, acceptance is by **subtyping** (`S <: R`), not by
  rewriting one concrete type tree to another.
- Do **not** model lambda result typing as a special-case conversion between two
  specific return types. If a case is valid, it must be valid because it
  follows the general checking/subtyping rule.
- Remember that `raise` is a **non-local return from the enclosing function**.
  Do not confuse that control-flow effect with the lambda's own ordinary result
  type.
- This means that `raise` must be affected by and affect the enclosing
  function's return type inference, even though it is lexically inside the
  lambda.

## Algebraic, Structural, and Generic Typing

- Verona types are **algebraic**: unions and intersections are real type
  constructors, not incidental containers.
- Verona types are **structural** in the presence of shapes. Matching a shape is
  about satisfying required structure, not about nominal identity.
- Verona types are **generic**. Type argument inference should be treated as a
  local constraint-solving problem driven by actual args, receiver types, field
  types, and expected results.
- Do not assume there is always a principal "best" type independent of context.
  With subtyping, intersections, and shapes, context is often essential.
- Prefer adding or repairing a propagation path over adding fallback defaults
  like `dyn` or `any`.

## Default Literals and Delayed Refinement

- Bare numeric literals begin as `DefaultInt` or `DefaultFloat`.
- They are placeholders that must be refined from nearby context:
  - call parameter types
  - variable annotations
  - field types in `new`
  - typed stores through refs
  - return expectations
  - backward propagation from later uses
- A runtime `bad type` often means a default literal remained unrefined in the
  compiler output even though enough context existed later.
- When changing default-literal logic, validate both compile and runtime
  behavior. A change that only moves a failure from compile time to runtime is
  not a fix.

## Control Flow and Narrowing

- Branch-local typetest narrowing belongs in branch-local environments.
- Join points must merge information conservatively and soundly.
- Return inference must prefer the more precise per-label environments over a
  coarser merged environment.
- Watch for oscillation when information can circulate through copies, moves,
  lookups, or call results. Use monotone propagation patterns when needed.
- `nomatch` is a control-flow artifact in source programs; do not treat it as
  semantic bottom.

## What Not To Do

- Do **not** mistake `nomatch` for bottom.
- Do **not** add a one-off special case for a single pair of concrete type
  trees when the real issue is missing contextual checking or subtyping.
- Do **not** broaden global subtype rules to patch a local inference bug.
- Do **not** collapse unresolved cases to `any` or `dyn` just to make a later
  pass accept the program.
- Do **not** ignore lambdas when tracing a supposedly "local" inference issue.
- Do **not** assume a compile-side improvement is enough; validate runtime too.

## Debugging Workflow

- Build with `ninja install`.
- Use `dist/vc/vc` and `dist/vbci/vbci`, not the non-installed build outputs.
- Dump passes with `--dump_passes=<dir>` and compare at least:
  - `07_anf.trieste`
  - `08_infer.trieste`
  - `09_reify.trieste`
  - `13_typecheck.trieste`
- When investigating a bug, check:
  - where the value first became default-typed or imprecise
  - where an expected type first became available
  - whether the needed backward edge exists
  - whether lambda context was propagated
  - whether `exit_envs` preserve branch narrowing
  - whether reify/typecheck is only exposing an earlier infer precision bug
- Compare a failing test with a nearby passing one to isolate the missing
  refinement path.

## Validation Checklist

- `cd build && ninja install`
- Reproduce with `dist/vc/vc build ../testsuite/v/<name> --dump_passes=<dir>`
- If the test compiles, also run `dist/vbci/vbci <file>.vbc`
- Run focused `ctest --output-on-failure -R '<pattern>'`
- Then run broader `ctest --output-on-failure -j$(nproc)`

