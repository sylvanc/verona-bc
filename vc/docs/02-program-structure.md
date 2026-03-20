# 2. Program Structure

[← Table of Contents](README.md) | [Previous: Getting Started](01-getting-started.md) | [Next: Types →](03-types.md)

This chapter describes the overall structure of a Verona source file and the fundamental syntactic rules.

---

## 2.1 Entry Point

Every executable Verona program must contain a `main` function that returns `i32`:

```verona
main(): i32
{
  0
}
```

The return value of `main` becomes the process exit code. This is the only required element — everything else is optional.

> **Command-line arguments**: There is currently no built-in mechanism for accessing `argc`/`argv`. Command-line argument support is planned as a future feature. For now, programs communicate results through exit codes and `:::printval`.

---

## 2.2 Top-Level Declarations

A `.v` source file contains a sequence of top-level declarations. The order of declarations does not matter — the compiler resolves names across the entire module before processing.

The following can appear at the top level:

| Declaration | Example |
|-------------|---------|
| Class | `point { x: i32; y: i32; }` |
| Shape (interface) | `shape drawable { draw(self: self); }` |
| Function | `add(a: i32, b: i32): i32 { a + b }` |
| Type alias | `use Name = TypeExpr` (inside a class) |
| Import | `use ModuleName` |
| FFI declaration | `use { func = "symbol"(types): ret; }` |

Classes, shapes, and functions are covered in detail in their respective chapters: [Classes](08-classes-and-objects.md), [Shapes](09-shapes.md), [Functions](07-functions.md).

---

## 2.3 Modules and Files

A project directory is a module. Each `.v` file inside it defines declarations that become part of that module — the file name does not create a separate module scope. Subdirectories create nested module scopes.

```
my_project/
  main.v       → declarations in the "my_project" module
  utils.v      → declarations in the "my_project" module
  helpers/
    format.v   → declarations in the "helpers" nested module
```

Declarations across all `.v` files in the same directory share one scope. Declarations in a nested directory are accessed using qualified names (`helpers::format_data()`) or by importing with `use helpers`. See [Modules and Imports](16-modules.md).

---

## 2.4 Comments

Verona supports two comment styles:

```verona
// Line comment — everything to end of line

/* Block comment — can be nested:
   /* inner comment */
   still in the outer comment
*/
```

Block comments are depth-counted, so nesting is safe. An unterminated block comment is an error.

---

## 2.5 Semicolons

Semicolons separate sequential statements within a block. The rules:

**Required between statements:**

```verona
main(): i32
{
  let a: i32 = 3;
  let b: i32 = 7;
  a + b
}
```

**Required after field definitions in classes:**

```verona
point
{
  x: i32;
  y: i32;
}
```

**Not required after the last expression in a block** — the last expression is the block's return value:

```verona
add(a: i32, b: i32): i32
{
  a + b          // no semicolon — this is the return value
}
```

> **What about a trailing semicolon?** Writing `{ a + b; }` is the same as `{ a + b }`. Unnecessary semicolons are ignored.

**Not required after closing braces of control flow:**

```verona
main(): i32
{
  var x = 0;

  while x < 10
  {
    x = x + 1
  }                // no semicolon after }

  if x == 10
  {
    x
  }
  else
  {
    0
  }                // no semicolon after }
}
```

---

## 2.6 No Standard Library

Verona deliberately has **no standard library** beyond `_builtin`. The `_builtin` module provides only primitive types (`i32`, `bool`, `string`, `array[T]`, etc.), basic shapes (`any`, `to_bool`), and fundamental operations.

Everything else — collections (hash maps, sets, growable lists), I/O, command-line argument parsing, string formatting, networking, file systems — is intended to be provided by **packages** imported via `use "url"` (see [Modules §16.8](16-modules.md)).

This is a deliberate design choice for **modularity**:

- **Embedded systems** can use only the primitives they need, with no unused library code.
- **Hypervisors and kernels** can operate without I/O or OS abstractions that don't apply.
- **Application programs** pull in exactly the packages they need from the ecosystem.

The goal is that Verona is usable across the full spectrum — from bare-metal embedded to cloud applications — without carrying unnecessary dependencies at any level. Even low-level details like collections and formatting are modular rather than built in.

> If you're looking for `HashMap`, `List`, `println`, or `format` — these will be package-level features, not language builtins.

---

## 2.7 Keywords

The following words are reserved: `use`, `where`, `let`, `var`, `if`, `else`, `while`, `match`, `when`, `break`, `continue`, `return`, `raise`, `new`, `shape`, `true`, `false`.

---

## 2.8 Identifiers

Identifiers start with a letter or underscore and contain letters, digits, and underscores:

```
[_a-zA-Z][_a-zA-Z0-9]*
```

Examples: `x`, `my_var`, `_internal`, `Point2D`.

The single underscore `_` is special — it is the discard (DontCare) token, not an identifier.
