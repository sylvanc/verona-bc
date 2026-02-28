# 9. Shapes (Structural Types)

[← Table of Contents](README.md) | [Previous: Classes and Objects](08-classes-and-objects.md) | [Next: Generics →](10-generics.md)

Shapes are Verona's mechanism for structural subtyping — interfaces defined by the methods a type must have, without requiring explicit `implements` declarations.

---

## 9.1 Shape Definitions

A shape declares method signatures without implementations:

```verona
shape drawable
{
  draw(self: self): none;
  bounds(self: self): rect;
}
```

Key rules:
- Method signatures in shapes have **no body** (prototypes only).
- The `self: self` parameter means "the concrete type that satisfies this shape."
- Shapes cannot be instantiated with `new`.
- Function prototypes (no body) are only allowed inside shapes.

---

## 9.2 Structural Conformance

A class satisfies a shape if it has all required methods with compatible signatures. No explicit declaration is needed:

```verona
shape valued
{
  val(self: self): i32;
}

box
{
  v: i32;

  val(self: box): i32 { self.v }
}

// box satisfies `valued` because it has val(self: box): i32
```

The conformance check happens during reification (monomorphization). If a class is used where a shape is expected and it lacks a required method, compilation fails.

---

## 9.3 The `self` Type

Inside a shape, `self` in type position refers to the concrete conforming type:

```verona
shape clonable
{
  clone(self: self): self;    // returns the same type as the receiver
}
```

When `point` satisfies `clonable`, the signature becomes `clone(self: point): point`.

---

## 9.4 The `any` Shape

The universal shape — satisfied by every class:

```verona
shape any {}
```

Used as a parameter type when any object is acceptable:

```verona
is(a: any, b: any): bool { :::eq(a, b) }
```

---

## 9.5 The `to_bool` Shape

Used by `bool`'s short-circuit operators:

```verona
shape to_bool
{
  apply(self: self): bool;
}
```

The `&` and `|` operators on `bool` take `to_bool` as the right-hand operand, enabling lazy evaluation. Any class with `apply(self: self): bool` can be used as the RHS of `&` or `|` on booleans.

---

## 9.6 Function Types as Shapes

Function types (`A -> B`) desugar to shapes with an `apply` method:

```verona
// This function type:
i32 -> i32

// Is equivalent to a shape like:
shape
{
  apply(self: self, arg: i32): i32;
}
```

This means any class with a matching `apply` method satisfies a function type:

```verona
apply_fn(f: i32 -> i32, x: i32): i32
{
  f(x)                       // calls f.apply(x)
}

// A lambda satisfies i32 -> i32:
let f = (x: i32): i32 -> { x + 1 };
apply_fn(f, 5)            // returns 6

// Any class with apply(self, i32): i32 also works
```

---

## 9.7 Generic Shapes

Shapes can have type parameters:

```verona
shape getter[T]
{
  get(self: self): T;
}
```

A class satisfies `getter[i32]` if it has `get(self: self): i32`:

```verona
box
{
  val: i32;
  get(self: box): i32 { self.val }
}

extract[T](g: getter[T]): T
{
  g.get
}

// extract(box(42)) → 42, with T inferred as i32
```

---

## 9.8 Recursive Shapes

Shapes can reference themselves in return types:

```verona
shape returner
{
  get(self: self): returner;
  val(self: self): i32;
}
```

A class satisfying `returner` must have a `get` method that returns something satisfying `returner` (possibly itself).

---

## 9.9 Using Shapes as Parameter Types

Shapes enable structural polymorphism:

```verona
shape printable
{
  to_string(self: self): string;
}

print_it(p: printable)
{
  // works with any class that has to_string(self): string
  let s = p.to_string;
  // ...
}
```

At each call site, the compiler monomorphizes a specialization for the concrete type passed. There is no runtime dispatch — shapes are resolved statically.

---

## 9.10 Shape Conformance Errors

If a class is used where a shape is expected but does not satisfy the shape, the compiler reports an error during monomorphization (the reify pass). The error indicates which method is missing or has an incompatible signature:

```verona
shape valued
{
  val(self: self): i32;
}

empty {}

use_valued(v: valued) { v.val }

main(): i32
{
  use_valued(empty())                 // error: empty does not satisfy valued
  // empty has no val(self: empty): i32 method
}
```

Shape conformance is checked at compile time — there is no runtime shape checking.

---

## 9.11 Multi-Parameter Shapes

Shapes can have multiple type parameters:

```verona
shape mapper[A, B]
{
  map(self: self, a: A): B;
}
```

A class satisfies `mapper[i32, string]` if it has `map(self: self, a: i32): string`.
