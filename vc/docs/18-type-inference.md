# 18. Type Inference

[← Table of Contents](README.md) | [Previous: FFI](17-ffi.md) | [Next: Memory Model →](19-memory-model.md)

This chapter describes how the Verona compiler infers types, with a focus on literal refinement and type argument inference.

---

## 18.1 Literal Type Defaults

Unadorned integer literals default to `u64`, and float literals to `f64`:

```verona
42        // u64 by default
3.14      // f64 by default
```

The infer pass refines these defaults based on the surrounding context, so you rarely need to write explicit type prefixes.

---

## 18.2 Inference from Function Parameters

When a literal is passed to a function, the parameter type drives inference:

```verona
add(a: i32, b: i32): i32 { a + b }

add(1, 2)                            // 1 and 2 refined to i32
```

Without a call context, `1` would be `u64`. In the call to `add`, the compiler sees that `a: i32` and refines the literal.

---

## 18.3 Inference from Variable Annotations

Explicit type annotations on variables refine their initializers:

```verona
var x: i32 = 0;                      // 0 refined to i32
let y: f32 = 3.14;                   // 3.14 refined to f32
```

---

## 18.4 Inference from Field Types

When constructing an object with `new`, field types refine literal arguments:

```verona
counter
{
  count: usize;

  create(): counter
  {
    new { count = 0 }                // 0 refined to usize from field type
  }
}
```

This also works with constructor sugar:

```verona
cell
{
  f: i32;
}

cell(42)                             // 42 refined to i32 from f: i32
```

---

## 18.5 Inference from Return Types

When a function has an explicit return type, the compiler refines the returned expression:

```verona
get_zero(): i32
{
  0                                  // refined to i32
}
```

---

## 18.6 Type Argument Inference

When calling a generic function, type arguments can be omitted if they can be inferred:

```verona
identity[T](x: T): T { x }
identity(i32 42)                     // T inferred as i32
```

### Through Wrapper Types

```verona
unwrap[T](w: wrapper[T]): T { w.get }
let w = wrapper[i32](42);
unwrap(w)                            // T inferred as i32 from w: wrapper[i32]
```

### Through Shape Matching

```verona
shape getter[T]
{
  get(self: self): T;
}

extract[T](g: getter[T]): T { g.get }

box
{
  val: i32;
  get(self: box): i32 { self.val }
}

extract(box(42))                     // T inferred as i32 by matching get() → i32
```

---

## 18.7 Backward Refinement

If later usage reveals the expected type, the compiler re-infers earlier expressions:

```verona
wrap[T](val: T): wrapper[T] { wrapper[T](val) }
unwrap_i32(w: wrapper[i32]): i32 { w.get }

// Initially: wrap(42) → T=u64 (default literal)
// After seeing unwrap_i32 expects wrapper[i32]:
// Backward-refine wrap(42) → T=i32, 42 refined to i32
unwrap_i32(wrap(42))
```

This is phase 4 of the infer pass — it handles cases where a downstream consumer reveals the type that an upstream producer should have used.

---

## 18.8 Array Literal Sibling Refinement

In array literals, if any element has an explicit type, other elements are refined to match:

```verona
let arr = ::(i32 1, 2, 3, 4);       // 2, 3, 4 refined to i32
```

The dominant non-default type is found and applied to all default-typed elements.

---

## 18.9 Cascade Propagation

When a literal is refined, the change cascades through assignments and operations:

```verona
var x: i32 = 0;                      // 0 → i32
let y = x + 1;                       // 1 → i32 (from x's type)
```

The infer pass tracks type information per variable and propagates refinements through Copy, Move, Lookup, RegisterRef, and New/Stack operations. When a literal changes type (e.g., `u64` → `i32` due to return type refinement), the cascade updates all downstream uses, including reference types (`ref[u64]` → `ref[i32]`) and anonymous class field types.

---

## 18.10 Lambda Parameter Inference

When a lambda is passed to a higher-order function, the compiler infers the lambda's parameter and return types from the expected function type.

### From Shape Types

Function types like `T -> none` desugar to shape classes with an `apply` method. When a lambda is passed where such a shape is expected, the compiler propagates the shape's `apply` signature to the lambda:

```verona
each(self: array[T], f: T -> none): none { ... }

let arr = array[i32]::fill(10);
arr.each i -> { process(i) }   // i: i32, inferred from T -> none
```

### Captured Variable Refinement

Captured `var` bindings create reference fields in the lambda class (e.g., `ref[TypeVar]`). The compiler propagates concrete types through the capture chain:

1. Return-type refinement resolves the `var`'s literal type in the enclosing scope
2. Cascade propagation updates the RegisterRef and the lambda's field type
3. The lambda's `apply` method is re-processed with the corrected field type

```verona
main(): i32
{
  var sum = 0;                        // default u64, refined to i32 by return type
  let arr = array[i32]::fill(10);
  arr.each i -> { sum = sum + i; none }  // sum: i32 inside lambda
  sum
}
```

---

## 18.11 When Inference Is Not Enough

If the compiler cannot infer a literal's type, prefix it explicitly:

```verona
i32 42                               // explicitly i32
f32 3.14                             // explicitly f32
usize 0                              // explicitly usize
```

This is rare in well-typed programs — inference handles most cases.

---

## 18.12 Explicit vs Inferred: Code Style

Idiomatic Verona relies on inference — prefer bare literals when the type is clear from context:

```verona
// Preferred — let inference handle it
add(1, 2)                            // refined to i32 from parameter types
var count: usize = 0;                // refined to usize from annotation
cell(42)                             // refined to i32 from field type

// Only when necessary — ambiguous context
i32 42                               // explicit when no context to infer from
```

Older test code in the repository may use explicit prefixes like `i32 0` where inference would suffice. Both styles compile correctly — the explicit form is never wrong, but the inferred form is preferred in new code.

---

## 18.13 Identified TypeVars and the Constraint Solver

Some generic functions cannot be reified by mechanical TypeArg substitution because the type parameter doesn't appear in any argument position — only in the return type or in the body's local annotations:

```verona
each_min[T, U](c: T): U | none
{
  var best: U | none = none;
  c.each (x: U) -> { best = x };
  best
}

each_min(some_vector_of_i32)         // U has no value to bind from c's type
```

The compiler binds `U` here by gathering subtype constraints during type checking and resolving them with a structured constraint solver.

### Identified TypeVars

Every `TypeVar` AST node carries a unique source `Location`. This is its identity. When the constraint solver records a fact about a TypeVar, the fact is keyed on this Location. References to the same TypeVar — across passes, across function boundaries, after `clone()` — share the Location, so constraints accumulate to the same root.

A `TypeName` resolving to a `TypeParam` is also treated as an identified TypeVar by the subtype machinery, using the `TypeParam`'s `Ident` Location. This lets formals participate in constraint emission without rewriting the AST.

### TypeVarStore

`TypeVarStore` (in `vc/typevar.{h,cc}`) is a structured store with:

- **Equivalence** via union-find on dense IDs assigned per first-seen Location. `unify(α, β)` merges classes.
- **Subtype lower bounds** `add_lower(α, T)` (i.e. `T <: α`).
- **Subtype upper bounds** `add_upper(α, T)` (i.e. `α <: T`).
- **Bind** `bind(α, T)` — shorthand for `add_lower ∧ add_upper`. Calls the occurs check.
- **Solve** `solve(α)` — returns the principal type: `lub(lowers)` if any, otherwise `glb(uppers)`, otherwise the empty `Union` (bottom). Memoized per generation; mutation bumps the generation.

A single global store accumulates constraints across passes (infer + reify). Identities are stable because `Location`s are global.

### Mode-aware subtype emission

`SequentCtx` (in `vbcc/sequent.h`) carries a `void* constraint_store` and an enum `Mode { Query, Emit }`:

- **`Query`** (default): atom rules behave as before — `TypeVar >> AxiomEq` answers true only on identity.
- **`Emit`**: when an atom rule encounters a TypeVar (or a TypeName-resolving-to-TypeParam) on either side, it emits the corresponding constraint and returns true (delayed proof). Wrappers in `vc/subtype.h` (`with_typevar(...)`) apply this preflight to every atom in the `Subtype` calculus.

Subtype callers choose the mode. Read-only queries (overload disambiguation, `definitely_not_subtype`) stay in `Query` so the store isn't polluted; emission paths (call-arg checking) switch to `Emit`.

### Where constraints come from

The infer pass emits at every Call / CallDyn arg-vs-formal subtype check (in `vc/passes/infer.cc`):

```cpp
SequentCtx emit_ctx{top, {}, {}};
emit_ctx.constraint_store = active_typevar_store;
emit_ctx.mode = SequentCtx::Mode::Emit;
(void)Subtype(emit_ctx, arg_it->second.type, expected);
```

For purely-generic callers where the receiver itself is a parametric formal (e.g. `each_min[T,U](c: T)` calling `c.each(...)`), infer can't yet fire — the receiver class isn't known at infer time. The reify pass runs the same kind of emission with the source AST and `r.subst` applied:

```cpp
emit_source_call_constraints(r.def, r.subst);
```

This walks the source body, tracks each local's source type, and at every Call/CallDyn navigates the callee's signature (FuncName for Call; receiver's ClassDef for CallDyn) to emit `Subtype(arg, formal)` with `Mode::Emit`. Because `r.subst` carries the α_k seeds for unbound formals, the decomposition reaches the formal-as-TypeVar atoms and records the bound.

### How reify consumes the bindings

For each unbound formal, reify:

1. Allocates an α_k whose Location is the `TypeParam`'s `Ident` Location.
2. Seeds `r.subst[TypeParam] = Type << TypeVar(α_k)`. `apply_subst` no longer skips TypeVar-valued entries — the substitution flows through into sub-trees so emitted constraints target the right identity.
3. Runs the source-call constraint emission pre-pass.
4. Queries `store.solve(α_k)` and overwrites the α_k seed in `r.subst` with the solved type. The body walk then proceeds with the concrete binding (so lambda New sites pick up the bound formal in their TypeArgs).
5. After the body walk, runs a late-solver consumption to catch any formals only bound through cross-reify gather of lambda apply bodies.

### Bottom is sound

A function with zero T-evidence (e.g. `f[T](): T | none = { none }`) reifies with α_T = ⊥ (empty Union). This is sound — the function never produces a T value, so any T is consistent.

### Hard error on unbound formals

After all reifications complete, reify scans the emitted IR for free `TypeVar` leaves. Any survivor is a hard error citing the type parameter source location. `Dyn` is reserved for the IR encoding of `any` and is never used as a fallback for unresolved formals.
