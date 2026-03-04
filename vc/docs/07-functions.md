# 7. Functions

[← Table of Contents](README.md) | [Previous: Control Flow](06-control-flow.md) | [Next: Classes and Objects →](08-classes-and-objects.md)

This chapter covers function definitions, methods, operator overloading, and related features.

---

## 7.1 Free Functions

A free function is defined with a name, parameters, return type, and body:

```verona
add(a: i32, b: i32): i32
{
  a + b
}
```

Free functions must be called with a qualified name or brought into scope with `use`:

```verona
// Qualified call
my_module::add(3, 4)

// After `use my_module`, unqualified is allowed
use my_module
add(3, 4)
```

This design prevents free functions from accidentally shadowing methods of the same name on other types. See [Modules and Imports](16-modules.md).

---

## 7.2 Methods

A function whose first parameter is named `self` with the class type is a method:

```verona
get(self: wrapper[T]): T
{
  self.val
}
```

The name `self` is not a keyword — it is a convention that the compiler recognizes. The compiler uses `self` when auto-generating field accessors and in several desugaring passes, so **always name the first parameter `self`** for methods.

Methods are called with dot syntax:

```verona
let w = wrapper[i32](42);
let v = w.get;                // calls get(self: wrapper[i32])
```

Zero-argument methods (only `self`) can be called without parentheses: `w.get` instead of `w.get()`.

Methods with additional arguments:

```verona
min(self: i32, other: i32): i32
{
  if self < other { self } else { other }
}

// Called as:
x.min(y)
```

### Method Self and Mutability

Inside a method, `self` is a regular parameter. Because field accessors return `ref[T]`, writing `self.field = val` always works — it goes through the `ref` field accessor and stores through the reference. See [Memory Model](19-memory-model.md).

---

## 7.3 Operator Overloading

Operators are methods whose names are symbol characters. They are defined and called like any method:

```verona
// Binary operator
+(self: i32, other: i32): i32
{
  :::add(self, other)
}

// Unary prefix operator
-(self: i32): i32
{
  :::neg(self)
}
```

Usage:

```verona
let sum = a + b;              // calls +(self: i32, other: i32)
let neg = -a;                 // calls -(self: i32)
```

The standard operators (`+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `&`, `|`, `^`, `<<`, `>>`, `!`) are defined on the built-in types.

### Idiomatic Operator Patterns

Implement the minimal set and derive the rest:

```verona
// Define < and ==, derive the others
<=(self: mytype, other: mytype): bool { !(other < self) }
>(self: mytype, other: mytype): bool { other < self }
>=(self: mytype, other: mytype): bool { !(self < other) }
!=(self: mytype, other: mytype): bool { !(self == other) }
```

---

## 7.4 Ref (LHS) Functions

A function prefixed with `ref` returns a mutable reference. This enables assignment syntax:

```verona
ref apply(self: array[T], index: usize): ref[T]
{
  :::arrayref(self, index)
}
```

With this definition, `arr(i) = val` works:

1. `arr(i)` on the left-hand side of `=` calls `ref apply`, returning a `ref[T]`.
2. The compiler emits a `Store` instruction that writes `val` through the reference.
3. The store performs a region-aware exchange — it updates reference counts, checks ownership rules, and returns the previous value.

Reading `arr(i)` on the right-hand side calls the regular (non-ref) `apply` method. If no explicit RHS `apply` exists, one is auto-generated that calls the `ref` version and loads the value.

### Complete Example: Field Access Flow

Given a class with a field:

```verona
point
{
  x: i32;
  y: i32;
}
```

The compiler auto-generates both a `ref` accessor and a value accessor:

```verona
// Auto-generated ref accessor (for writes):
ref x(self: point): ref[i32] { :::fieldref(self, x) }

// Auto-generated value accessor (for reads):
x(self: point): i32 { /* load through ref x */ }
```

Now field access works bidirectionally:

```verona
let p = point(1, 2);
let v = p.x;                 // calls x(self: point): i32 → reads 1
p.x = 10;                    // calls ref x(self: point): ref[i32] → stores 10
```

See [Memory Model §19.6](19-memory-model.md) for how stores work through `ref[T]`.

---

## 7.5 Default Arguments

Parameters can have default values:

```verona
create(f: i32 = 0): cell
{
  new {f}
}
```

The default is used when the argument is omitted:

```verona
let a = cell;                // calls create(f: 0) — uses default
let b = cell(7);             // calls create(f: 7) — overrides default
```

Another example with the `fill` function on arrays:

```verona
fill(size: usize, from: T = T): array[T]
```

```verona
array[i32]::fill(10)              // default-filled (T::create())
array[i32]::fill(3, 42)           // filled with 42
```

Default arguments must appear at the end of the parameter list.

> **Note:** Verona does not have named arguments at call sites. Arguments are matched positionally. The parameter names in function signatures (like `from`) exist only in the function definition.

---

## 7.6 Return Type Omission

If a function has no explicit return type, the compiler infers it:

```verona
double(x: i32) { x + x }     // return type inferred as i32
```

The [infer pass](18-type-inference.md) resolves unspecified return types from the function body.

---

## 7.7 Auto-Generated `create`

If a class has no explicit `create` function, one is auto-generated from its fields:

```verona
point
{
  x: i32;
  y: i32;
}

// Auto-generated:
// create(x: i32, y: i32): point { new { x = x, y = y } }
```

Usage:

```verona
let p = point(1, 2);
```

If the class defines any `create` function, no auto-generation occurs.

---

## 7.8 Overload Resolution

Functions are resolved by:

1. **Name** — the function name
2. **Arity** — the number of arguments
3. **Handedness** — LHS (`ref`) vs. RHS (regular)

There is no overloading by parameter type alone — only name and arity matter for resolution. Type checking happens after resolution.

### Name Resolution Rules

- **Methods**: Called via dot syntax. The compiler looks in the receiver's class.
- **Qualified functions**: `Module::func(args)` — looked up directly in the specified module.
- **Unqualified names**: If a name cannot be resolved as a local variable, class, or type alias by walking up scopes, it becomes a dynamic method dispatch (`CallDyn`).
- **Free functions are NOT resolved by walking up scopes** — this is intentional. It prevents a free function like `!=` in an outer scope from shadowing `!=` on a type in an inner scope. Use `use` or qualified names for free functions.

---

## 7.9 Once Functions (Memoized)

A function prefixed with `once` is memoized — it is evaluated exactly once at program startup (before `main()`), and all subsequent calls return the cached result:

```verona
once answer(): i32
{
  i32 42
}

main(): i32
{
  let a = my_module::answer();
  let b = my_module::answer();       // returns the same cached value
  a + b                               // 84
}
```

### Rules

- **Zero arguments only**: `once` functions cannot have parameters. A `once` function with parameters is a compile error.
- **No methods**: Because Verona methods always have at least one parameter (`self`), the zero-parameter rule naturally prevents `once` on methods.
- **Mutually exclusive with `ref`**: `once` and `ref` occupy the same grammar slot — you cannot combine them.
- **Eager evaluation**: `once` functions run before `main()`, in dependency order. If `once f()` calls `once g()`, then `g` is evaluated first.
- **Cycle detection**: Circular dependencies between `once` functions (including through non-once intermediaries) are detected at compile time and reported as errors.
- **Immortal results**: The cached return value is held for the program's entire lifetime and never collected. This is appropriate for the primary use case (global cowns and configuration).

### Dependencies

`once` functions can call other `once` functions. The compiler performs a topological sort to determine evaluation order:

```verona
once base(): i32
{
  i32 10
}

once derived(): i32
{
  my_module::base() + i32 32
}

main(): i32
{
  my_module::derived()                // returns 42
}
```

### Compile Errors

```verona
// ERROR: once functions must have no parameters
once bad(x: i32): i32 { x }

// ERROR: circular dependency
once f(): i32 { my_module::g() }
once g(): i32 { my_module::f() }
```

### Primary Use Case: Global Singletons

`once` is designed for creating global singleton objects initialized before `main()`:

```verona
external
{
  c: cown[none];

  once create(): external
  {
    new {c = cown none}
  }
}
```

This pattern (from `_builtin/ffi/notify.v`) creates a single shared instance with a cown field. All callers of `ffi::external()` get the same instance.
