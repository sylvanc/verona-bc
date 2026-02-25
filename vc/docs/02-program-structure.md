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

Each `.v` file defines a module named after the file (without the extension). A directory of `.v` files is treated as an implicit class scope — its files are members.

```
my_project/
  main.v       → module "main"
  utils.v      → module "utils"
```

Declarations in one module can reference declarations in another using qualified names (`utils::helper()`) or by importing with `use utils`. See [Modules and Imports](16-modules.md).

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

## 2.6 Keywords

The following words are reserved:

**Implemented:** `use`, `where`, `let`, `var`, `if`, `else`, `while`, `for`, `when`,
`break`, `continue`, `return`, `new`, `shape`

**Reserved for future use:** `try`, `raise`, `throw` — see [Error Handling](24-error-handling.md)

---

## 2.7 Identifiers

Identifiers start with a letter or underscore and contain letters, digits, and underscores:

```
[_a-zA-Z][_a-zA-Z0-9]*
```

Examples: `x`, `my_var`, `_internal`, `Point2D`.

The single underscore `_` is special — it is the discard (DontCare) token, not an identifier.
