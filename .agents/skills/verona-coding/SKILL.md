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
- **Annotate the variable, not the literal**: write `var i: i32 = 0`, not
  `var i = i32 0`. Verbose `T LIT` forms in source signal that the author
  was working around an old inference gap; in most cases inference now
  works through the var's declared type.
- **Backward refinement works through method-arg context** for concrete
  receivers. `m.set(K, V)` on `hmap[i32, i32]` refines bare `m.set(1, 7)`
  to `i32` for both args. Same for `v.push(X)`, `v.insert(idx, val)`,
  `arr(idx) = val`. Drop redundant `i32 N` annotations at the call site.
- **Backward refinement does NOT cross typeparam-receivers**. For a generic
  `c: Src` whose `c.size` has typeparam-typed return, `let n = c.size`
  leaves `n` unannotated and bare `1` in `n <= 1` defaults to `u64`. Fix:
  annotate the local — `let n: usize = c.size`. The annotation pins the
  type and lets the literal refine.
- **Array literal sibling refinement**: `::(i32 10, 20, 30, 40, 50)` —
  the first element typed serves as anchor; the infer pass propagates
  the dominant non-default type to siblings. Drop trailing `i32` on
  every element except the first.
- **Array literal type annotation does NOT propagate**: `let src: array[i32]
  = ::(2, 3, 4)` is rejected — the array literal defaults to `array[u64]`
  and fails the subtype check against the annotation. Use the first-element
  anchor pattern instead.
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
- **Don't write empty `else {}`** branches when the if has no else action:
  use `if cond { ... }` alone. `if cond { ... } else {};` adds a useless
  branch and a non-idiomatic `;`.
- **Use constructor sugar `Type(args)`** instead of `Type::create(args)`.
  Juxtaposition on a type name calls its `create` method.

## Statement terminators (`;`) and closing braces (`}`)

This is the most common source of non-idiomatic style. Get it right.

**Hard rule**: `};` is **always** non-idiomatic — the `;` is never
necessary after `}`. A closing `}` terminates an expression. The
only continuation allowed is `else` (which is part of an `if` /
`match` chain).

This applies in EVERY context, including:
- Statement form: `if cond { ... }` — no `;` after `}`.
- Statement form with else: `if cond { ... } else { ... }` — no `;`
  after the final `}`.
- Match (statement or expression): `match x { (p) -> body } else { d }`
  — no `;`.
- Lambda calls: `c.method (x: T) -> { ... }` — no `;`.
- `when` blocks: `when c w -> { ... }` — no `;`.
- **Even in let/var bindings**: `let x = if cond { 1 } else { 2 }` —
  the `}` of the else terminates the whole expression AND the `let`
  statement. No `;` needed before the next statement.
- **Inside lambda bodies / blocks**: `{ if cond { f() } none }` —
  no `;` between the `if` block and `none`.

**Statements between curly braces** within a block:
- Non-block statements (assignments, method calls without lambda
  args, `let`/`var` of bare values) ARE separated by `;` from the
  next statement. `let x = compute(); let y = other();`.
- A statement that ends with `}` (control flow, lambda call, etc.)
  needs NO `;` before the next statement. The `}` is the separator.

**Last expression of a block has no `;`** — it IS the value of the
block:
```
fn(): T
{
  do_setup();           // ; (non-block stmt, more follows)
  let x = compute();    // ; (let of bare value, more follows)
  if cond { early() }   // no ; — } terminates
  let y = match z       // no ; — } terminates
  {
    (a: A) -> a.thing
  }
  else { default }
  x + y                 // no ; — final value
}
```

## Syntax Reminders

- `};` is **always** non-idiomatic. `}` terminates an expression.
  The only thing that may follow `}` is `else` (continuing an if/match
  chain).
- Semicolons after non-block statements and field definitions, when
  not last in a block.
- `new { field = val }` — no class name after `new`.
- `Type(args)` is sugar for `Type::create(args)`. Always prefer the sugar.
  For zero args, drop both — `vector[i32]` not `vector[i32]()`.
- `array[u8]::fill(n)` allocates an array of size `n` (zero-filled).
- `self.len` calls the getter (zero-arg method, no parens needed).
- `f()` calls a zero-arg callable value. Don't drop the parens on lambda/function
  values just because zero-arg methods omit them.
- Free function calls require parens: `algo::tests()` not `algo::tests`
  (the latter parses as a name reference and errors).
- Prefer paren-less syntax for clear zero-arg and single-arg calls:
  `handle::signal`, `tty`, `self.init handler`, `pipe::open 0`.
- `self.len = x` calls the setter.
- `final(self: T)` receives a read-only self. Don't call mutating helpers like
  `self.close` from a finalizer; inline read-only-safe cleanup instead.

## Performance

- **Use bulk operations over byte-by-byte loops**: `array.copy_from()` wraps
  `memmove` (handles overlapping regions). Use it for shifting data within the
  same array instead of while loops.
- **Avoid double-moving data**: `replace` should shift the tail once to its
  final position and copy the replacement in, not call `erase` then `insert`
  (which shifts the tail twice).
- **`copy_from` for self-overlapping copies**: `self.data.copy_from(dst, self.data, src, len)`
  works correctly for overlapping regions — it's `memmove`, not `memcpy`.

## Naming conventions for sequence collections

Single naming convention across all sequence-shaped collections
(vector, list, deque, span — anything with positional access):

- **Accessors** for ends: `front` and `back`. Not `first`/`last`.
  These return `T | none` for empty containers.
- **Mutators** for ends: `push_front`/`pop_front`/`push_back`/`pop_back`.
  Always end-explicit, even for collections that only support one end
  (e.g., `vector` only has `push_back`/`pop_back`, not bare `push`/`pop`).
- **`pop_*`** returns the removed element as `T | none`.

Span is a non-owning view: it has the **accessors** (`front`, `back`,
`apply(i)`, `size`, `is_empty`, `each`) but **NOT** push/pop
(those imply data ownership/destruction). For sub-spans, span uses
`drop_front(n)`/`drop_back(n)`/`take_front(n)`/`take_back(n)` —
view arithmetic, not push/pop semantics.

This convention:
- Matches C++ STL (`front()`/`back()` everywhere).
- Internally consistent: a deque's `front` accessor lines up with its
  `push_front`/`pop_front` mutators.
- Lets a function written against `front`/`back`/`apply`/`size`/`each`
  accept any of vector, list, deque, span uniformly (shape compatibility).

## Unified iteration shape: every collection's `each` yields `(K, V)`

A single iteration shape across all collections, eliminating the
`each` vs `pairs` split:

- **Sequences** (vector, list, deque, span, `_builtin::array`):
  `each(f: (usize, T) -> none)` — first arg is index, second is value.
- **Sets** (set, oset): `each(f: (usize, T) -> none)` — first arg is
  iteration index, second is element.
- **Maps** (hmap, omap): `each(f: (K, V) -> none)` — first arg is key,
  second is value.

There is **no separate `pairs` method**. The pair-shape IS the only
iteration shape.

**Why this design (vs separate `each` + `pairs`):**
- One shape across all collections → one algo per operation
  (no `_pairs` variants of map/filter/etc.).
- Index/key always available without "retrofit" (avoids the
  `enumerate()` / `entrySet()` patterns of Rust/Java/C#/Python).
- Verona's multi-arg lambda types (`(K, V) -> none` is a 2-arg
  function type, NOT a tuple-arg) make this natural — the lambda
  is `(k, v) -> body`, not `((k, v)) -> body`.

**Cost:** value-only ops on sequences carry an unused first arg
(`(_i: usize, x: T) -> ...`). Use `_i` / `_k` (underscore-prefixed
names — bare `_` is reserved for tuple destructuring and isn't a
valid lambda param name).

**Algo functions:**
- All `algo::*` functions take `(K, T) -> ...` shape lambdas.
- Aggregators that operate on a single value (sum, product, min,
  max, contains, fold, reduce) bind both `(k, x)` in the iteration
  but ignore `k` — the lambda parameter convention is `(k: K, x: T)`.
- Search functions (`find`, `find_last`, `position`) return `K | none`
  (the address) — for sequences this is `usize`, for maps it's the
  key type. This naturally generalizes "position" semantics across
  collections.
- `algo::map(c, (k, x) -> u)` produces a `mapview` whose iteration
  yields `(K, U)` — keys preserved, values transformed.
- `algo::filter` keeps the `(K, V)` shape — predicate decides
  retention.

**Materialization** (collection → fresh array/vector) lives on the
target type, not on algo:
- `vector[T]::from_each[Src, K](src)` builds a `vector[T]` from any
  `each`-providing source. Keys dropped.
- `array[T]::from_each[Src, K](src)` same for arrays.

For key projection (e.g., `keys_of(m)`):

```
let ks = vector[K]::from_each(algo::map(m, (k: K, v: V): K -> k))
```

## Constants and Empty Values

- **`none`** — no parens needed. Write `none` not `none()`.
- **Don't use `()` unnecessarily**: if a type or value needs no arguments,
  omit the parens. `none`, `true`, `false`, not `none()`, `true()`, `false()`.
- **Empty string**: `string(array[u8]::fill(1))` — a 1-byte array containing
  just the null terminator, with `len=0`.

## Constructor calls (zero-arg)

- **A type used as a value with no args needs no parens**: write
  `vector[i32]` not `vector[i32]()`. The `Type(args)` constructor sugar
  reduces to bare `Type` when there are no arguments — same rule as
  `none` vs `none()`. This applies for:
  - `vector[i32]`, `hmap[K, V]`, `set[T]`, `oset[T]`, `omap[K, V]`,
    `deque[T]`, `list[T]`, `span[T]`, etc.
  - Module-qualified: `collections::vector[i32]`, `set::oset[T]`.
  - Nested: `hmap[bool, vector[i32]]`.
- **Distinguish from free-function calls**: `algo::tests()` (a free
  function call) DOES need parens — it's an invocation, not a type
  reference. The compiler errors with "Expected at least one argument
  to this method" if you drop parens on a free-function call.
- **Method calls on `self` follow the zero-extra-arg rule**:
  `self.size`, `self.is_empty` (no parens). See "Field Access vs
  Method Call" above.

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

## FFI / libuv Wrapper Lifetimes

- **Tie keepalive to the real external lifetime**: for libuv-backed wrappers,
  use timer-style `_activate(active)` plus handler-safe deferred release
  (`_finish_handler`) so `ffi::pin` / `ffi::unpin` and
  `ffi::external.add` / `ffi::external.remove` track active handles or pending
  requests.
- **Don't rely on incidental references**: a callback closure or unrelated
  signal watcher may accidentally keep a wrapper alive for a while, but that is
  not a sound lifetime strategy.
- **Synchronous setup failures need their own path**: if a wrapper can fail
  before it becomes active (for example `pipe.open(fd)`), don't route the error
  through an active-only dispatch helper — use a separate ungated failure
  callback path.
- **Capture sendable wrappers in FFI callbacks**: when a callback must refer
  back to an owned state machine, capture the outer sendable wrapper (or another
  sendable handle), not a cown-owned mutable `_state` directly.
