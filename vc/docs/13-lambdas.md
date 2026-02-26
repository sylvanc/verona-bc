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

```verona
x -> { x + 1 }
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
{ i32 42 }
```

---

## 13.2 Using Lambdas

Lambdas are values that can be bound to variables and passed as arguments:

```verona
let f = (x: i32): i32 -> { x + 1 };
let result = f(5);               // 6

// Passing to a higher-order function:
apply_fn(f: i32 -> i32, x: i32): i32
{
  f(x)
}

apply_fn((x: i32): i32 -> { x * 2 }, 5)   // 10
```

---

## 13.3 Closures (Capturing Variables)

Lambdas capture variables from the enclosing scope:

```verona
main(): i32
{
  let x: i32 = 5;
  let f = (y: i32): i32 -> { y + x };    // captures x
  f(1)                                // 6
}
```

Multiple captures:

```verona
let offset: i32 = 10;
let scale: i32 = 2;
let f = (x: i32): i32 -> { x * scale + offset };
f(3)                                 // 16
```

### Capturing Type Parameters

Lambdas inside generic functions can capture type parameters:

```verona
apply_with_offset[T](val: T, offset: T): wrapper[T]
{
  let f = (x: T): wrapper[T] -> { wrapper[T](offset) };
  f(val)
}
```

The type parameter `T` is automatically captured. Internally, it becomes a type parameter of the lambda's anonymous class.

---

## 13.4 How Lambdas Desugar

Lambdas desugar to anonymous classes with an `apply` method. For example:

```verona
let offset: i32 = 10;
let f = (x: i32): i32 -> { x + offset };
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

## 13.5 Lambdas and Function Types

A lambda satisfies any function type (`A -> B`) if its signature matches:

```verona
// i32 -> i32 is a shape with apply(self: self, arg: i32): i32
let f: i32 -> i32 = (x: i32): i32 -> { x + 1 };
```

This works because function types desugar to shapes (see [Shapes](09-shapes.md)), and the lambda's anonymous class has a matching `apply` method.

---

## 13.6 Shadowing in Lambdas

Lambda parameters can shadow outer variables:

```verona
let x: i32 = 10;
let f = (x: i32): i32 -> { x + 1 };  // this x is the parameter, not the outer
f(5)                               // 6, not 11
```

---

## 13.7 Block Lambdas and `raise`

A **block lambda** is a lambda that contains a `raise` expression. `raise` performs a non-local return — it exits not just the lambda, but the enclosing function that created it:

```verona
find_first(a: i32, b: i32, target: i32): i32
{
  let check = (x: i32) -> {
    if x == target
    {
      raise x                         // returns directly from find_first
    }
  };
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

## 13.8 Type Inference for Lambdas

Lambda parameter and return types can be inferred from context:

```verona
// When passed to a function expecting i32 -> i32:
apply_fn((x) -> { x + 1 }, 5)
// x is inferred as i32, return type inferred as i32
```

See [Type Inference](18-type-inference.md).
