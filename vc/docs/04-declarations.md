# 4. Declarations

[← Table of Contents](README.md) | [Previous: Types](03-types.md) | [Next: Expressions →](05-expressions.md)

This chapter covers variable declarations — how to introduce names and bind values.

---

## 4.1 Let Bindings (Immutable)

`let` introduces a single-assignment binding. Once bound, the name cannot be reassigned to a different value:

```verona
let x = 42;
let name = "hello";
```

With an explicit type annotation:

```verona
let x: i32 = 42;
```

When a type annotation is provided, the literal on the right is inferred to match (e.g., `42` becomes `i32` rather than the default `u64`). See [Type Inference](18-type-inference.md).

> **Important:** `let` constrains the **binding**, not the **object**. You can still mutate the fields of a `let`-bound object:
>
> ```verona
> let p = point(1, 2);
> p.x = 10;                           // allowed — mutates the object, not the binding
> // p = point(3, 4);                 // NOT allowed — can't reassign the name
> ```
>
> See [Memory Model](19-memory-model.md) for more on binding vs object mutability.

### Assignment Is Aliasing

Assignment in Verona creates an **alias** — a second name for the same object. It does not copy or move the value:

```verona
let a = point(1, 2);
let b = a;                            // b is an alias for the same object as a
b.x = 10;                            // a.x is now also 10
```

Both `a` and `b` refer to the same underlying object. Mutating through one name is visible through the other. There is no implicit copy, no move that invalidates the source, and no borrow — just two names for the same thing.

This applies to all types (classes, arrays, strings). Primitive operations like `a + b` produce new values, but assignment itself always aliases.

---

## 4.2 Var Bindings (Mutable)

`var` introduces a reassignable binding:

```verona
var count = 0;
count = count + 1;
```

With an explicit type annotation:

```verona
var count: i32 = 0;
count = count + 1;
```

### `var` Without Initializer

A `var` can be declared with a type but no initial value:

```verona
var x: i32;
x = 42;                              // assign it later
```

The variable is uninitialized until assigned. Similarly, `let x: i32;` is valid and must be assigned exactly once before use.

> **What happens if you read before assignment?** Reading an uninitialized variable does not immediately error — the variable holds an internal `Invalid` marker. However, **using** the value in any operation (arithmetic, function call, comparison, field access) produces a runtime error. The error you see depends on context: `bad type` for a function call with the wrong type, `bad operand` for arithmetic, etc. To avoid confusion, always assign before use.

### Common Error: Reassigning a `let` Binding

Attempting to reassign a `let` binding produces a compile error:

```verona
let x = 42;
x = 10;                              // error: cannot reassign a let binding
```

The compiler will report an error during the structure pass. Use `var` if you need reassignment.

---

## 4.3 Discard (`_`)

The underscore `_` discards a value. Use it when an expression must be evaluated but its result is not needed:

```verona
_ = some_side_effect();
```

`_` can also appear in tuple destructuring to ignore specific elements:

```verona
let t = (3, 5);
(_, let b) = t;              // b = 5, first element discarded
(let a, _) = t;              // a = 3, second element discarded
(_, _) = t;                  // discard everything
```

Use `_...` to discard all remaining elements regardless of count:

```verona
let t = (1, 2, 3, 4, 5);
(let a, _...) = t;           // a = 1, rest discarded
```

See [Tuples and Destructuring](11-tuples.md) for more on destructuring and splat patterns.

---

## 4.4 Scoping

Bindings are scoped to the block they are declared in. Inner blocks can shadow outer bindings:

```verona
main(): i32
{
  let x: i32 = 10;
  let y = if true
  {
    let x: i32 = 20;          // shadows outer x
    x                        // 20
  }
  else
  {
    x                        // 10
  };
  y                          // 20
}
```

---

## 4.5 Declarations as Expressions

`let` and `var` declarations are expressions that evaluate to the bound value. This allows them to appear inside tuple destructuring and other expression contexts:

```verona
(let a, let b) = (3, 5);              // a = 3, b = 5
(var x, let y) = (1, 2);              // x = 1 (mutable), y = 2
```
