---
name: verona-coding
description: Patterns, pitfalls, and idioms for writing Verona source code (.v files). Use when writing library code, _builtin types, or user programs.
---

# Verona Coding Skill

Patterns, pitfalls, and idioms for writing Verona source code. Use this skill
when writing `.v` files — library code, `_builtin` types, or user programs.

## Literal Types and Inference

- **Avoid explicit literal constructors** (e.g., `u64 0`, `i32 3`). Let type
  inference determine the type from context. If inference fails, that's a
  compiler bug to fix — report it rather than working around it.
- **Known inference gap**: Union return types (e.g., `usize | none`) don't
  refine bare literals. `return 0` in a function returning `usize | none`
  gives `u64`, not `usize`. This is a compiler bug to be fixed.
- **`compare()` returns `i64`**: comparing with bare `0` works — inference
  resolves from the `i64` return type of `compare()`.

## Operators

- **No operator precedence**: All infix operators evaluate left-to-right.
  Operators, methods, and functions are all the same thing — you can use
  non-symbolic names in the infix position too (e.g., `a min b`).
- **Consequence**: `cap < n + 1` evaluates as `(cap < n) + 1`. Use parens
  to group: `cap < (n + 1)`.
- **Juxtaposition binds tighter than infix**: `a(i)` binds before any infix
  operator, so `sum + a(i)` works as expected.
- **Bool or is `|`**, bool and is `&`. Since there's no precedence,
  parenthesize each comparison: `(c == 32) | (c == 9) | (c == 10)`.
- **Free functions need qualification**: `string::is_space(c)`, not
  `is_space(c)`. Unqualified names resolve as method calls on the first arg.

## Field Access vs Method Call

- **Dot consumes arguments**: `self.data(i)` calls method `data` with arg `i`,
  NOT field access + apply.
- **Zero-arg getters don't need `()`**: Write `self.data` not `self.data()`,
  `self.size` not `self.size()`. Parens are unnecessary for zero-extra-arg
  methods (only `self`).
- **Field-then-apply**: `self.data()(i)` — the first `()` is needed to
  disambiguate from `self.data(i)` (which calls method `data` with arg `i`).
  But when chaining further: `self.data.pairs ...` — no parens needed on
  `data` because `pairs` is consumed by dot, not by juxtaposition.
- **For local variables**: `result(i)` works directly (no field access ambiguity).
- **`ref` keyword on functions**: `ref apply(...)` returns a `ref[T]`, enabling
  both read and write through the result. Use `ref self.data()(index)` to
  delegate ref access.

## Lambda Syntax and Higher-Order Functions

- **Lambda syntax**: `(params) -> { body }`. Examples:
  - `(x: i32) -> { x + 1 }`
  - `(i, c) -> { ... }` (types inferred from context)
  - `{ body }` (zero-arg lambda)
- **Passing lambdas to methods**: Use juxtaposition, NOT wrapping parens.
  The lambda IS the argument:
  ```
  self.data.pairs (i, c) -> {
    ...
  }
  ```
  NOT `self.data.pairs((i, c) -> { ... })` — the extra parens are unnecessary.
- **No trailing semicolon** when a lambda call is a statement by itself
  (the `}` ends the expression).
- **`raise` in a lambda** is a non-local return — it exits the ENCLOSING
  function, not the lambda. The lambda's own return type is unaffected by
  `raise`.

## String Literals and Immutability

- **Verona has deep immutability via `freeze`** — any object can be mutable or
  permanently immutable. This is NOT like C++ `const`.
- **String literals should be frozen**, not copied. The compiler wraps ConstStr
  in `string::create()` and then freezes the result. The frozen string directly
  references the constant pool array — no copy.
- **`create()` should NOT copy the input array**. It just wraps it:
  ```
  create(data: array[u8]): string
  {
    new { data, len = data.size - 1 }
  }
  ```
- **Mutation on a frozen string** is caught naturally — you can't mutate an
  immutable object. If you want a mutable string, explicitly create one.
- **The copy-on-create approach is C++ thinking**. Verona's model: keep the
  original, make it immutable, copy only when mutability is needed.

## Character Literals

- **Space literal `' '` causes parser issues** in nested control flow. Use the
  numeric value `32` instead. Other char literals like `'a'` work fine.
- Common ASCII values: space=32, tab=9, newline=10, carriage return=13.

## Union Return Types

- **`usize | none` is a union type**. Callers consume it with match/else:
  ```
  match s.find("x") { (i: usize) -> i; } else { 99 }
  ```
- **Returning a loop variable from a union-return function** can cause the
  variable to become `dyn`. Copy to a typed `let` first (see Literal Types
  section above).

## Style

- **Match expression syntax** — format like control flow, not inline:
  ```
  match expr
  {
    (pattern) -> body;
  }
  else
  {
    default
  }
  ```
  Not `match expr { (pattern) -> body; } else { default }` on one line.
- **Don't wrap expressions in unnecessary parens**: only use parens when needed
  for grouping (e.g., operator precedence).

## Performance

- **Use bulk operations over byte-by-byte loops**: `array.copy_from()` wraps
  `memmove` (handles overlapping regions). Use it for shifting data within the
  same array instead of while loops.
- **Avoid double-moving data**: `replace` should shift the tail once to its
  final position and copy the replacement in, not call `erase` then `insert`
  (which shifts the tail twice).
- **`copy_from` for self-overlapping copies**: `self.data.copy_from(dst, self.data, src, len)`
  works correctly for overlapping regions — it's `memmove`, not `memcpy`.



- **`none`** — no parens needed. Write `none` not `none()`.
- **Don't use `()` unnecessarily**: if a type or value needs no arguments,
  omit the parens. `none`, `true`, `false`, not `none()`, `true()`, `false()`.
- **Empty string**: `string(array[u8]::fill(1))` — a 1-byte array containing
  just the null terminator, with `len=0`.

## Syntax Reminders

- No semicolons after closing braces of `if`/`while`/`for`.
- Semicolons after statements and field definitions.
- `new { field = val }` — no class name after `new`.
- `Type(args)` is sugar for `Type::create(args)`.
- `array[u8]::fill(n)` allocates an array of size `n` (zero-filled).
- `self.len` calls the getter (zero-arg method, no parens needed).
- `self.len = x` calls the setter.

## Testing Patterns

- Use bitmask accumulation for multi-check tests:
  ```
  var result = 0;
  if cond1_fails { result = result + 1 }
  if cond2_fails { result = result + 2 }
  // ...powers of 2...
  result   // exit code 0 means all passed
  ```
- Tests must be self-contained — no external dependencies, no `use "_builtin"`.
