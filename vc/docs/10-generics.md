# 10. Generics

[← Table of Contents](README.md) | [Previous: Shapes](09-shapes.md) | [Next: Tuples →](11-tuples.md)

This chapter covers type parameters, monomorphization, and type argument inference.

---

## 10.1 Type Parameters on Classes

Classes can be parameterized by one or more type parameters in brackets:

```verona
wrapper[T]
{
  val: T;

  create(val: T): wrapper[T]
  {
    new { val }
  }

  get(self: wrapper[T]): T
  {
    self.val
  }
}
```

Usage:

```verona
let w = wrapper[i32](42);        // explicit type argument
let v = w.get;                    // v: i32
```

---

## 10.2 Type Parameters on Functions

Functions can also be generic:

```verona
identity[T](x: T): T
{
  x
}

wrap[T](val: T): wrapper[T]
{
  wrapper[T](val)
}
```

---

## 10.3 Type Argument Inference

When the compiler can determine type arguments from the actual argument types, they can be omitted:

```verona
// Explicit:
identity[i32](i32 42)

// Inferred — T=i32 from the argument type:
identity(i32 42)
```

Inference also works through generic parameter types:

```verona
unwrap[T](w: wrapper[T]): T
{
  w.get
}

let w = wrapper[i32](42);
unwrap(w)                         // T inferred as i32 from w: wrapper[i32]
```

### Inference Through Shapes

When a parameter type is a shape, the compiler can infer type arguments by matching the shape's method signatures against the concrete type:

```verona
shape getter[T]
{
  get(self: self): T;
}

extract[T](g: getter[T]): T
{
  g.get
}

box
{
  val: i32;
  get(self: box): i32 { self.val }
}

// extract(box(42)) — T inferred as i32 by matching get() return type
```

### Backward Refinement

If a later usage reveals the expected type, the compiler can backward-refine earlier inferences:

```verona
unwrap_i32(w: wrapper[i32]): i32
{
  w.get
}

// wrap(42) initially infers T=u64 (default literal type)
// but unwrap_i32 expects wrapper[i32], so the compiler
// backward-refines wrap's T to i32
unwrap_i32(wrap(42))
```

See [Type Inference](18-type-inference.md) for full details on how inference works.

---

## 10.4 Monomorphization

Generics are resolved at compile time by the reify pass. Each distinct combination of type arguments produces a separate specialization:

```verona
wrapper[i32]    → specialized wrapper class for i32
wrapper[string] → specialized wrapper class for string
```

There is no runtime generic dispatch — all generics are fully resolved before code generation. The compiler starts from `main()` and transitively reifies all reachable generic instantiations.

---

## 10.5 Shape Bounds

Type parameters can be constrained by shapes using `where` clauses:

```verona
sort[T](arr: array[T]) where T < comparable
{
  // T must satisfy the comparable shape
}
```

The `where` clause uses `<` for subtype constraints, with `&` (and), `|` (or), and `!` (not) for combining constraints. See [Types](03-types.md).

---

## 10.6 Explicit Type Arguments

When inference cannot determine a type argument (or you want to be explicit), provide them in brackets:

```verona
let a = array[i32]::fill(10);
let w = wrapper[string]("hello");
```

Type arguments appear after the class or function name, before `::` or `(`:

```verona
ClassName[TypeArg]::method(args)
function_name[TypeArg](args)
```
