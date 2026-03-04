# 13. Lambdas and Closures

[← Table of Contents](README.md) | [Previous: Arrays](12-arrays.md) | [Next: Partial Application →](14-partial-application.md)

This chapter covers lambda expressions, closures, and how they interact with the type system.

---

## 13.1 Lambda Syntax

Lambdas are anonymous functions written with the `->` arrow:

### Full Form

```verona
(x: i32, y: i32): i32 -> { x + y }
```

### Single Parameter (no parens needed)

When a lambda has a single parameter with no type annotation, no default value, and no return type, you can omit the parentheses:

```verona
x -> { x + 1 }
```

If you need to specify a parameter type, a default value, or a lambda return type, use parentheses:

```verona
(x: i32) -> { x + 1 }                // typed parameter needs parens
(x: i32): i32 -> { x + 1 }           // typed parameter + return type
(x = 0) -> { x + 1 }                 // default value needs parens
```

### With Type Annotations

```verona
(x: i32): i32 -> { x + 1 }
```

### Without Types (inferred)

```verona
(x) -> { wrapper[T](x) }
```

### No Parameters

A block `{ expr }` in expression position is a zero-argument lambda:

```verona
{ 42 }
```

---

## 13.2 Trailing Lambda Syntax

When the argument to a method call is a lambda, you can omit parentheses on the call, just like for any single argument call:

```verona
// These are equivalent:
arr.each((x: i32) -> { process(x) })
arr.each (x: i32) -> { process(x) }
arr.each x -> { process(x) }
```

Since `each` expects a function argument, the lambda is passed as that argument.

This is especially useful for higher-order methods like `each` and `pairs`:

```verona
let arr = array[i32]::fill(10);

// Using pairs with trailing lambda (two parameters need parens):
arr.pairs (i, v) -> { arr(i) = i.i32 }

// Using each with trailing lambda (single parameter, no parens):
var sum = 0;
arr.each i -> { sum = sum + i }
```

---

## 13.3 Using Lambdas

Lambdas are values that can be bound to variables and passed as arguments:

```verona
let f = (x: i32): i32 -> { x + 1 }
let result = f(5);               // 6

// Passing to a higher-order function:
apply_fn(f: i32 -> i32, x: i32): i32
{
  f(x)
}

apply_fn((x: i32): i32 -> { x * 2 }, 5)   // 10
```

---

## 13.4 Closures (Capturing Variables)

Lambdas capture variables from the enclosing scope:

```verona
main(): i32
{
  let x: i32 = 5;
  let f = (y: i32): i32 -> { y + x }    // captures x
  f(1)                                  // 6
}
```

Multiple captures:

```verona
let offset: i32 = 10;
let scale: i32 = 2;
let f = (x: i32): i32 -> { x * scale + offset }
f(3)                                 // 16
```

### Capturing `let` vs `var` Bindings

The capture mechanism differs for `let` and `var` bindings:

- **`let` captures** copy the value into a field of the lambda class. Inside the lambda body, accesses become field reads and writes on `self`.
- **`var` captures** capture a **reference** (`ref[T]`) to the variable. Inside the lambda body, reads become loads through the reference and writes become stores, so changes are visible in the enclosing scope.

```verona
main(): i32
{
  let arr = array[i32]::fill(3);     // let-capture: copied into lambda
  var sum = 0;                        // var-capture: captured by reference

  arr.each i -> { sum = sum + i }
  sum                                 // visible changes from lambda body
}
```

### Capturing Type Parameters

Lambdas inside generic functions can capture type parameters:

```verona
apply_with_offset[T](val: T, offset: T): wrapper[T]
{
  let f = (x: T): wrapper[T] -> { wrapper[T](offset) }
  f(val)
}
```

The type parameter `T` is automatically captured. Internally, it becomes a type parameter of the lambda's anonymous class.

---

## 13.5 How Lambdas Desugar

Lambdas desugar to anonymous classes with an `apply` method. For example:

```verona
let offset: i32 = 10;
let f = (x: i32): i32 -> { x + offset }
```

Becomes (conceptually):

```verona
lambda$0
{
  offset: i32;

  apply(self: lambda$0, x: i32): i32
  {
    x + self.offset
  }
}

let f = lambda$0(offset);
```

Captured variables become fields. Captured type parameters become type parameters of the anonymous class.

---

## 13.6 Lambdas and Function Types

A lambda satisfies any function type (`A -> B`) if its signature matches:

```verona
// i32 -> i32 is a shape with apply(self: self, arg: i32): i32
let f: i32 -> i32 = (x: i32): i32 -> { x + 1 }
```

This works because function types desugar to shapes (see [Shapes](09-shapes.md)), and the lambda's anonymous class has a matching `apply` method.

---

## 13.7 Shadowing in Lambdas

Lambda parameters can shadow outer variables:

```verona
let x: i32 = 10;
let f = (x: i32): i32 -> { x + 1 }  // this x is the parameter, not the outer
f(5)                                // 6, not 11
```

---

## 13.8 Block Lambdas and `raise`

A **block lambda** is a lambda that contains a `raise` expression. `raise` performs a non-local return — it exits not just the lambda, but the enclosing function that created it:

```verona
find_first(a: i32, b: i32, target: i32): i32
{
  let check = (x: i32) -> {
    if x == target
    {
      raise x                         // returns directly from find_first
    }
  }
  check(a);
  check(b);
  0
}
```

When `raise x` executes inside `check`, control returns directly from `find_first` with value `x`.

### How It Works

1. When a lambda containing `raise` is **created**, it captures the current raise target — a reference to the enclosing function's stack frame.
2. When the lambda is **called** and `raise` executes, it restores the captured target and performs a non-local return to the enclosing function's caller.
3. The raised value becomes the return value of the enclosing function.

### Rules

- `raise` can only appear inside a lambda. Using it outside a lambda is a compile error.
- The raise target is captured at lambda creation time, not at call time.
- Any lambda containing `raise` is a block lambda — there is no separate syntax to mark it.

See [Error Handling](24-error-handling.md) for more examples and the full error handling picture.

---

## 13.9 Type Inference for Lambdas

Lambda parameter and return types can often be inferred from context.

### From Higher-Order Function Signatures

When a lambda is passed to a function that expects a specific function type, the compiler propagates the expected parameter and return types to the lambda:

```verona
each(self: array[T], f: T -> none): none { ... }

let arr = array[i32]::fill(10);
arr.each i -> { process(i) }   // i inferred as i32 from T -> none
```

This works because the `each` method's `f` parameter has type `T -> none`, which desugars to a shape with `apply(self: self, a0: T): none`. When `T` is `i32`, the compiler propagates `i32` to the lambda's parameter.

### From Call Context

```verona
// When passed to a function expecting i32 -> i32:
apply_fn((x) -> { x + 1 }, 5)
// x is inferred as i32, return type inferred as i32
```

### Captured Variable Types

Captured `var` bindings have their types propagated through the lambda's anonymous class fields. If the enclosing function's return type constrains the `var`'s type (via cascade propagation), the lambda body sees the refined type:

```verona
main(): i32
{
  var sum = 0;                        // 0 defaults to u64, but...
  // return type i32 refines sum to i32
  // cascade propagates through ref capture to lambda
  arr.each i -> { sum = sum + i }
  sum                                 // i32
}
```

See [Type Inference](18-type-inference.md) for the full inference picture.

---

## 13.10 Case Lambdas (Match Arms)

Match expressions desugar to **case lambdas** — each match arm becomes a lambda that tests a value and either returns a result or `nomatch`.

### Type Test Arms

A type test arm is a lambda whose parameter has a type annotation:

```verona
(x: i32) -> { x + 1 }
```

This tests whether the value matches `i32`. If it does, `x` is bound to the value and the body executes. Otherwise, the arm returns `nomatch`.

### Value Test Arms

A value test arm is a lambda whose parameter is an expression (not a typed identifier):

```verona
(42) -> { 100 }
```

This compares the input value against `42` using `==`. If they're equal, the body executes. If `==` doesn't exist for those types or the types don't match, the arm returns `nomatch` (no runtime error).

### Bare Identifier Ambiguity

In lambda parameters, a bare identifier always parses as a parameter definition (with inferred type), never as a case value expression. To use a variable as a case value, wrap it in an expression that isn't an identifier:

```verona
// This defines a parameter named 'x', it does NOT compare against variable x:
(x) -> { ... }

// To compare against a specific value, use a literal or typed expression:
(42) -> { ... }
(i32 42) -> { ... }
```

This is a deliberate parsing choice — type-free and unambiguous.

### How Case Lambdas Work

Under the hood, case lambdas use `TryCallDyn` for value tests — a fallible variant of dynamic dispatch that returns `nomatch` if the method doesn't exist or argument types don't match, rather than crashing. This makes value matching safe even when the case value's type has no `==` method or when comparing across incompatible types.

See [Control Flow §6.8](06-control-flow.md) for the full match expression syntax.
