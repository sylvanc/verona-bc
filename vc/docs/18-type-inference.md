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
arr.each i -> { process(i); none }   // i: i32, inferred from T -> none
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
