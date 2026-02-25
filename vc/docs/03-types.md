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

---

## 3.3 Union Types

A union type `A | B` represents a value that is either of type `A` or type `B`:

```verona
next(self: arrayiter[T]): T | nomatch
```

Union types are left-associative and can have more than two alternatives:

```verona
A | B | C       // a value that is A, B, or C
```

Union types are used extensively in iterators (returning `T | nomatch`) and in `when` blocks.

### Discriminating Union Types

There is no general-purpose `match` statement or `if x is Type` syntax. Union type discrimination is done through the `for` loop and `else` mechanism:

- **`for` loops** automatically handle `T | nomatch`: when the iterator’s `.next()` returns `nomatch`, the loop exits. The user never sees the `nomatch` case.
- **`else` on `for` iteration**: Internally, the `for` loop desugaring uses `it.next() else { break }` — the `else` branch runs when the expression evaluates to `nomatch`.

For functions returning `T | nomatch`, the idiomatic pattern is to use the result in a `for`-compatible way or to design APIs so that `nomatch` is handled implicitly.

---

## 3.4 Intersection Types

An intersection type `A & B` represents a value that satisfies both `A` and `B`:

```verona
A & B           // must satisfy both A and B
```

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

Type aliases can be generic (with type parameters) and can have `where` clauses.

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

`where` clauses constrain type parameters:

```verona
sort[T](arr: array[T]) where T < comparable
```

The `<` operator in a `where` clause means **structural subtyping** — `T < comparable` requires that `T` satisfies the shape `comparable` (i.e., `T` has all the methods that `comparable` declares).

Supports logical connectives:

| Connective | Meaning | Example |
|------------|---------|--------|
| `<` | Satisfies shape | `T < comparable` |
| `&` | Both constraints | `T < comparable & T < printable` |
| `\|` | Either constraint | `T < serializable \| T < printable` |
| `!` | Negation | `!T < numeric` |

Where clauses are checked during monomorphization — when a generic function or class is instantiated with concrete type arguments, the compiler verifies that the constraints are satisfied.
