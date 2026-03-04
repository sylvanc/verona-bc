# 26. Gotchas and Pitfalls

[← Table of Contents](README.md) | [Previous: Compile-Time Execution](25-compile-time-execution.md)

This chapter collects the most common surprises for programmers coming from other languages.

---

## 26.1 Flat Operator Precedence

**All infix operators have the same precedence.** There is no precedence difference between `+`, `*`, `<`, `==`, `&`, `|`, or any user-defined operator. Expressions are parsed left-to-right:

```verona
a + b * c                    // parsed as (a + b) * c — NOT a + (b * c)
a * b + c                    // parsed as (a * b) + c
a < b & c > d                // parsed as ((a < b) & c) > d
```

**Always use parentheses** to express the intended grouping:

```verona
a + (b * c)                  // multiplication first
(a < b) & (c > d)            // comparisons first, then AND
```

This is a deliberate design choice. Since all operators are method calls and users can define new operators, there is no principled reason for `*` to bind tighter than a user-defined `%%`. Flat precedence makes the rules simple and uniform.

See [Expressions §5.8](05-expressions.md) for the complete binding order.

---

## 26.2 `let` Does Not Freeze Objects

`let` constrains the **binding** (the name), not the **object**. A `let`-bound object is still mutable — you can write to its fields:

```verona
let p = point(1, 2);
p.x = 10;                            // allowed — mutates the object
// p = point(3, 4);                  // NOT allowed — can't reassign the name
```

| Binding | Can reassign the name? | Can mutate the object? |
|---------|------------------------|------------------------|
| `let` | No | Yes |
| `var` | Yes | Yes |

If you want true immutability, region freezing will provide it (see [Memory Model §19.9](19-memory-model.md)) — but it is not yet implemented.

See [Declarations §4.1](04-declarations.md) and [Memory Model §19.4](19-memory-model.md).

---

## 26.3 Assignment Returns the Previous Value

Assignment (`=`) is an expression that evaluates to the **previous value** of the left-hand side, not the new value:

```verona
var x = 1;
let old = x = 2;                     // old is 1 (not 2), x is now 2
```

This is different from C, Rust, Python, and most other languages where `x = 2` evaluates to `2` (or is a statement with no value).

### Why?

Assignment-returns-old enables two powerful idioms:

**Swap:** `a = b = a` swaps the values of `a` and `b` in a single expression:

```verona
var a = 1;
var b = 2;
a = b = a;                           // a is now 2, b is now 1
```

How it works: `b = a` stores `a`'s value (1) into `b` and returns `b`'s old value (2). Then `a = 2` stores that into `a`.

**Destructive read:** `a = b = c` reads `b`'s current value and replaces it in one step:

```verona
var a: i32 = 0;
var b: i32 = 42;
a = b = 0;                           // a is now 42, b is now 0
```

These patterns will become especially important when regions are fully implemented — they enable ownership transfers without temporary variables.

See [Expressions §5.13](05-expressions.md) and [Memory Model §19.5](19-memory-model.md).

---

## 26.4 No `class` Keyword

Classes are declared with a bare name — no `class`, `struct`, or `type` keyword:

```verona
point
{
  x: i32;
  y: i32;
}
```

This can be surprising if you're scanning code looking for `class` declarations. In Verona, any bare name followed by `{` at the top level (or inside another class) is a class definition.

See [Classes and Objects](08-classes-and-objects.md).

---

## 26.5 Dot Consumes Arguments

`obj.method(args)` calls the method `method` with `args`. It does **not** call `apply` on the result of accessing `obj.method`:

```verona
x.f(y)                               // calls method f on x with arg y
```

To call `apply` on a field's value, use parentheses or explicit `.apply`:

```verona
(x.f)(y)                             // calls apply on the value of field f
x.f.apply(y)                         // same thing, explicit
```

This matters when a field holds a callable value (a lambda or an object with `apply`). See [Expressions §5.8](05-expressions.md).

---

## 26.6 For Loops Need `->`

The `for` loop requires the arrow `->` after the element binding:

```verona
for arr.values() elem ->
{
  // body
}
```

Forgetting `->` is a common syntax error. The arrow separates the lambda parameter from the body — `for` loop bodies are lambdas under the hood.

---

## 26.7 Free Functions Must Be Qualified or Imported

Free functions are **not** resolved by walking up scopes. You must either qualify them or import with `use`:

```verona
my_module
{
  helper(): i32 { 42 }
}

main(): i32
{
  // helper()                         // ERROR — not found
  my_module::helper()                 // OK — qualified
}
```

Or:

```verona
use my_module

main(): i32
{
  helper()                            // OK — imported via use
}
```

This prevents a free function like `!=` from accidentally shadowing a method of the same name on a type. See [Modules §16.5](16-modules.md).

---

## 26.8 Integer Literals Default to `u64`

Unadorned integer literals are `u64`, not `i32` or `int`. Float literals are `f64`:

```verona
42                                    // u64, not i32
3.14                                  // f64
```

The compiler's type inference usually refines these from context (function parameters, variable annotations, field types). But in ambiguous contexts, you may need an explicit type:

```verona
i32 42                                // explicitly i32
```

Or rely on inference from the surrounding context:

```verona
let x: i32 = 42;                     // 42 inferred as i32
add(1, 2)                            // 1 and 2 inferred from add's parameter types
```

See [Type Inference](18-type-inference.md).

---

## 26.9 No Semicolons After `}`

Control flow blocks (`if`, `while`, `for`, `match`, `when`) do **not** need a semicolon after the closing brace:

```verona
if x > 0
{
  do_something()
}

next_statement();                     // no ; needed after the } above
```

But statements within a block **do** need semicolons between them:

```verona
let a = 1;
let b = 2;
a + b
```

See [Program Structure §2.5](02-program-structure.md).

---

## 26.10 Match Without `else` Returns `nomatch`

A `match` expression without an `else` clause includes `nomatch` in its result type:

```verona
let result = match x { (n: i32) -> n; };
// result type: i32 | nomatch
```

This is not an error — it's by design. Use `else` to handle the no-match case and strip `nomatch` from the type:

```verona
let result = match x { (n: i32) -> n; } else (0);
// result type: i32
```

Exhaustiveness checking is not yet implemented — the compiler does not verify that all possible types are covered by match arms.

See [Control Flow §6.8](06-control-flow.md).

---

## 26.11 `_builtin` Is Always Available

You never need to write `use "_builtin"`. The `_builtin` module (containing all primitive types like `i32`, `string`, `bool`, etc.) is implicitly imported by the compiler.

See [Modules §16.4](16-modules.md).

---

## 26.12 Recursive Types Need Union with `none`

Recursive data structures require a union type to terminate the recursion:

```verona
node
{
  val: i32;
  next: node | none;
}
```

Without the `| none`, the type would be infinitely recursive. The `none` type (from `_builtin`) acts as the base case. To traverse:

```verona
main(): i32
{
  let b = node(2, none::create());
  let a = node(1, b);
  a.val
}
```

See [Union Types §3.3](03-types.md) and [Special Types §3.2](03-types.md).

---

## 26.13 The Four Faces of `use`

The `use` keyword is overloaded — it means different things depending on what follows it:

| Syntax | Meaning |
|--------|---------|
| `use math` | Import a module for unqualified lookup |
| `use "url"` | Import a remote package |
| `use x = "url"` | Import a package with a name |
| `use { ... }` | FFI declarations |

The forms are syntactically distinct, so there's no ambiguity — but seeing `use` in four different roles can be surprising. See [Modules §16.8](16-modules.md) for details.

---

## 26.14 `once` Values Are Immortal

The return value of a `once` function is cached for the program's entire lifetime — it is never garbage collected. This is by design for the primary use case (global singleton cowns), but it means you should not use `once` for large temporary data structures. See [Functions §7.9](07-functions.md).
