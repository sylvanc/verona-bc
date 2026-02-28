# 14. Partial Application

[← Table of Contents](README.md) | [Previous: Lambdas](13-lambdas.md) | [Next: Concurrency →](15-concurrency.md)

This chapter covers partial application using `_` (DontCare) placeholders.

---

## 14.1 Syntax

Use `_` in a function or method call to create a partially applied function. The `_` placeholders become parameters of the resulting function:

```verona
// Given:
add(a: i32, b: i32): i32 { a + b }

// Partial applications:
let add5 = add(5, _);            // fix first arg → (i32) -> i32
let add3 = add(_, 3);            // fix second arg → (i32) -> i32
```

---

## 14.2 Using Partial Applications

The result of partial application is a callable value:

```verona
let add5 = add(5, _);
add5(3)                           // 8

let add3 = add(_, 3);
add3(10)                          // 13
```

---

## 14.3 Multiple Placeholders

Multiple `_` placeholders create a function with multiple parameters:

```verona
three_args(a: i32, b: i32, c: i32): i32 { a + b + c }

let f = three_args(_, 10, _);         // (i32, i32) -> i32
f(1, 2)                               // 13

// All placeholders — equivalent to a reference to the function
let g = add(_, _);                    // (i32, i32) -> i32
g(4, 5)                               // 9
```

---

## 14.4 Method Partial Application

`_` works with method calls via dot syntax:

```verona
counter
{
  value: i32;

  add(self: counter, n: i32): i32
  {
    self.value + n
  }

  combine(self: counter, a: i32, b: i32): i32
  {
    self.value + a + b
  }
}

let c = counter(100);

let h = c.add(_);                     // captures c, placeholder for n
h(5)                                  // 105

let j = c.combine(10, _);            // fix first arg
let k = c.combine(_, 20);            // fix second arg
let m = c.combine(_, _);             // all args as placeholders
```

The receiver object is captured along with any non-placeholder arguments.

---

## 14.5 How Partial Application Works

Partial application desugars to anonymous classes (the same mechanism as [lambdas](13-lambdas.md)):

```verona
let add5 = add(5, _);
```

Becomes (conceptually):

```verona
partial$0
{
  captured_a: i32;

  apply(self: partial$0, b: i32): i32
  {
    add(self.captured_a, b)
  }
}

let add5 = partial$0(5);
```

Non-placeholder arguments are captured as fields. Placeholder positions become parameters of the `apply` method.

---

## 14.6 Type Inference

Types for `_` placeholders and the resulting return type are inferred from the target function's signature:

```verona
add(a: i32, b: i32): i32 { a + b }
let f = add(_, _);                    // inferred as (i32, i32) -> i32
```

This includes type parameter substitution for generic functions.

---

## 14.7 DontCare on the Left-Hand Side

`_` on the left-hand side of an assignment discards the value (see [Declarations](04-declarations.md)):

```verona
_ = some_function();                  // evaluate and discard
(_, let b) = some_tuple();           // destructure, discard first
```

This is separate from partial application — `_` in a call creates a closure, while `_` in an assignment target discards.
