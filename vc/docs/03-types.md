# 3. Types

[← Table of Contents](README.md) | [Previous: Program Structure](02-program-structure.md) | [Next: Declarations →](04-declarations.md)

This chapter describes the type system, including primitive types, composite types, and type-level operations.

---

## 3.1 Primitive Types

### Integer Types

| Type | Size | Range |
|------|------|-------|
| `i8` | 8-bit signed | -128 to 127 |
| `i16` | 16-bit signed | -32,768 to 32,767 |
| `i32` | 32-bit signed | -2³¹ to 2³¹-1 |
| `i64` | 64-bit signed | -2⁶³ to 2⁶³-1 |
| `ilong` | Platform-width signed | Matches C `long` |
| `isize` | Pointer-width signed | Matches C `ptrdiff_t` |
| `u8` | 8-bit unsigned | 0 to 255 |
| `u16` | 16-bit unsigned | 0 to 65,535 |
| `u32` | 32-bit unsigned | 0 to 2³²-1 |
| `u64` | 64-bit unsigned | 0 to 2⁶⁴-1 |
| `ulong` | Platform-width unsigned | Matches C `unsigned long` |
| `usize` | Pointer-width unsigned | Matches C `size_t` |

### Floating Point Types

| Type | Size | Description |
|------|------|-------------|
| `f32` | 32-bit | IEEE 754 single precision |
| `f64` | 64-bit | IEEE 754 double precision |

### Other Primitives

| Type | Description |
|------|-------------|
| `bool` | Boolean — `true` or `false` |
| `string` | String — backed by `array[u8]` |

All primitive types are defined in the `_builtin` module. See [Built-in Types Reference](22-builtin-types.md) for complete method listings.

---

## 3.2 Special Types

| Type | Description |
|------|-------------|
| `none` | The unit type. Has a single value, created by `none::create()`. Used when a function has no meaningful return value. |
| `nomatch` | Sentinel for failed pattern matches and iterator exhaustion. Returned by `.next()` on exhausted iterators. |
| `any` | The universal shape — `shape any {}`. Satisfied by every class. Used for identity-agnostic operations like `is()` and `bits()`. |
| `ptr` | Opaque raw pointer type. Empty class with no methods. |

### No Null

Verona has no `null`. If a value might be absent, use a union type with `none`:

```verona
node
{
  val: i32;
  next: node | none;                  // nullable field — explicitly modeled
}
```

This is the equivalent of `Optional<T>` in Java, `T?` in TypeScript/Kotlin, or `Option<T>` in Rust. The compiler requires you to handle the `none` case (via `match` or `else`) before using the value as the non-none type.

### `none` vs `nomatch`

These two types serve distinct purposes and should not be confused:

**`none`** is the unit type — analogous to `void` in C/C++, `()` in Rust, or `None` in Python. It represents "no meaningful value" but is still a real value you can pass around:

```verona
log(msg: string): none
{
  :::printval(msg)
}
```

**`nomatch`** is a sentinel that signals "this operation did not produce a result." It is used by:
- **If expressions**: `if cond { ... }` returns `nomatch` if the condition is false.
- **The `else` mechanism**: `expr else { fallback }` handles the `nomatch` case.
- **Match expressions**: A failed match arm returns `nomatch`, which flows to the next arm or the `else` fallback.

```verona
// none: "I did something, here's the unit value"
let x: none = log("hello");

// nomatch: "I couldn't find what you asked for"
let result: i32 | nomatch = match v { (n: i32) -> n; }
```

Key difference: `none` is a successful return with no data. `nomatch` is a signal that the operation failed to match or find something. Code that consumes `T | nomatch` uses `else` to branch on exhaustion or failure; a `none`-returning helper would not trigger that fallback.

---

## 3.3 Union Types

A union type `A | B` represents a value that is either of type `A` or type `B`:

```verona
next(self: pair_iter): (usize, i32) | nomatch
```

Union types are left-associative and can have more than two alternatives:

```verona
A | B | C       // a value that is A, B, or C
```

Union types are used extensively in fallible operations (returning `T | nomatch`) and in `when` blocks.

### Discriminating Union Types

Union type discrimination is done through `match` expressions and `else` on expressions:

- **`match` expressions** test a value against type patterns and value patterns. Each arm is a case lambda — type test arms bind the value to a typed parameter, value test arms compare using `==`. See [Control Flow §6.8](06-control-flow.md) for full details.
- **`else` on expressions**: `expr else { fallback }` handles the `nomatch` case — the `else` branch runs when the expression evaluates to `nomatch`.

```verona
// Type discrimination with match:
let x: i32 | string = get_value();
let result = match x
{
  (n: i32) -> n + 1;
  (s: string) -> s.size;
}
else
{
  0
}
```

---

## 3.4 Intersection Types

An intersection type `A & B` represents a value that satisfies both `A` and `B`:

```verona
A & B           // must satisfy both A and B
```

Intersection types are used in `where` clauses to constrain type parameters:

```verona
// T must satisfy both comparable and printable
format[T](val: T) where T < comparable & T < printable
```

An intersection type is also formed when combining shape constraints — a value of type `A & B` has all the methods of both `A` and `B`. This is useful for requiring that a type satisfies multiple shapes simultaneously.

Intersection types are left-associative:

```verona
A & B & C       // a value satisfying A, B, and C
```

See [Where Clauses §3.10](#310-where-clauses) for how intersection types work with generic constraints.

---

## 3.5 Function Types

Function types use the arrow syntax `A -> B`:

```verona
i32 -> i32                    // takes i32, returns i32
(i32, i32) -> bool            // takes two i32, returns bool
() -> i32                     // takes nothing, returns i32
i32 -> (i32 -> i32)           // returns a function (right-associative)
```

Function types are right-associative: `A -> B -> C` means `A -> (B -> C)`.

Under the hood, function types desugar to shapes with an `apply` method. For example, `i32 -> i32` becomes:

```verona
shape { apply(self: self, arg: i32): i32; }
```

This means any class with a compatible `apply` method satisfies a function type. See [Shapes](09-shapes.md) and [Lambdas](13-lambdas.md).

---

## 3.6 Tuple Types

Tuple types are written with parenthesized, comma-separated types:

```verona
(i32, i32)                    // pair of i32
(i32, string, bool)           // triple
```

Tuples are heterogeneous — each element can have a different type. See [Tuples and Destructuring](11-tuples.md).

---

## 3.7 Type Aliases

Type aliases give a name to a type expression. They are declared inside a class body using `use`:

```verona
my_module
{
  use IntPair = (i32, i32);
  use Predicate[T] = T -> bool;
}
```

Type aliases can be generic (with type parameters). Type aliases can appear in any class scope, including at the top level of a module (since a module directory is itself a class scope).

---

## 3.8 Type Conversions

Values are converted between types using dot syntax — calling a conversion method named after the target type:

```verona
let x: usize = 42;
let y: i32 = x.i32;          // convert usize to i32
let z: f64 = y.f64;          // convert i32 to f64
```

Every numeric type defines conversion methods to every other numeric type. See [Built-in Types Reference](22-builtin-types.md).

---

## 3.9 The `self` Type

In shape definitions, `self` in type position refers to the concrete type that satisfies the shape:

```verona
shape comparable
{
  <(self: self, other: self): bool;
}
```

When `i32` satisfies `comparable`, `self` is `i32`. The `self` type is only valid inside shape definitions.

---

## 3.10 Where Clauses

> **Note:** `where` clauses are parsed by the compiler but not yet fully enforced. The `<` subtype constraint has partial support (checked at monomorphization time via shape matching). The `&`, `|`, and `!` connectives are parsed but their enforcement is a planned feature.

`where` clauses constrain type parameters:

```verona
sort[T](arr: array[T]) where T < comparable
```

The `<` operator in a `where` clause means **structural subtyping** — `T < comparable` requires that `T` satisfies the shape `comparable` (i.e., `T` has all the methods that `comparable` declares).

The syntax supports logical connectives:

| Connective | Meaning | Example |
|------------|---------|--------|
| `<` | Satisfies shape | `T < comparable` |
| `&` | Both constraints | `T < comparable & T < printable` |
| `\|` | Either constraint | `T < serializable \| T < printable` |
| `!` | Negation | `!T < numeric` |

### Combined Constraints

Multiple constraints can be combined to require a type parameter to satisfy several shapes:

```verona
// T must satisfy both comparable and printable
format_sorted[T](arr: array[T]) where T < comparable & T < printable
{
  sort(arr);
  arr.each elem ->
  {
    print_it(elem)
  }
}
```

### Constraints on Multiple Type Parameters

Each type parameter can have its own constraints:

```verona
transform[A, B](input: A, mapper: mapper[A, B]) where A < printable & B < printable
```

In practice, type parameters are currently checked at monomorphization time — when a generic function or class is instantiated with concrete type arguments, the compiler verifies that the concrete type has the required methods. Explicit constraint enforcement from `where` clauses is planned.
