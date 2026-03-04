# 6. Control Flow

[← Table of Contents](README.md) | [Previous: Expressions](05-expressions.md) | [Next: Functions →](07-functions.md)

This chapter covers conditional expressions, loops, and early exit.

---

## 6.1 If / Else

`if` is an expression — it evaluates to a value:

```verona
let x = if condition { 1 } else { 0 }
```

### Basic Forms

```verona
if condition { body }

if condition { body } else { alternative }

if condition { body }
else if condition2 { body2 }
else { fallback }
```

### Single-Expression Else

The `else` branch can use parentheses for a single expression:

```verona
if false { 0 }
else if false { 1 }
else (2)
```

### As a Statement

When used as a statement (not capturing the result), no semicolon is needed after the closing brace:

```verona
if x > 0
{
  do_something()
}

next_statement();
```

---

## 6.2 While Loops

```verona
while condition
{
  body
}
```

The condition is evaluated before each iteration. The loop exits when the condition is `false`.

```verona
var i = 0;
while i < 10
{
  arr(i) = i.i32;
  i = i + 1
}
```

---

## 6.3 For Loops

`for` iterates over an iterator:

```verona
for iterator element -> { body }
```

The iterator must have a `next()` method returning `T | nomatch`. The loop calls `.next()` each iteration and binds the result to `element`. When `.next()` returns `nomatch`, the loop exits.

```verona
for arr.values() i ->
{
  sum = sum + i
}
```

### How It Desugars

```verona
for iter elem -> { body }
```

is equivalent to:

```verona
{
  let it = iter;
  while true
  {
    let elem = it.next() else { break }
    body
  }
}
```

### Multiple Parameters

For loop parameters can destructure tuples:

```verona
for iter (a, b) -> { body }
```

---

## 6.4 Break and Continue

`break` exits the innermost loop:

```verona
while true
{
  if done { break }
}
```

`continue` skips to the next iteration:

```verona
while i < 10
{
  i = i + 1;
  if skip_condition { continue }
  process(i)
}
```

Both `break` and `continue` are statements.

---

## 6.5 Raise (Non-Local Return)

`raise` performs a non-local return from a block lambda back to the enclosing function. The raised value becomes the result of the enclosing call expression:

```verona
find_first(a: i32, b: i32, target: i32): i32
{
  let check = (x: i32) -> {
    if x == target
    {
      raise x
    }
  }
  check(a);
  check(b);
  0
}
```

When `raise x` executes inside the lambda, control returns directly from `find_first` with value `x` — not just from the lambda.

### Rules

- `raise` can only appear inside a lambda body. Using `raise` outside a lambda is a compile error.
- The lambda captures the raise target (the enclosing function's frame) at creation time.
- When called, `raise` restores the captured target and returns directly to the enclosing function's caller.
- `raise` is the only non-local control flow mechanism — there is no `try`/`throw` or exception system.
- If a lambda containing `raise` escapes the enclosing function and is called after that function has returned, the result is a runtime error. Use `raise` only in immediately-invoked block lambdas.

See [Error Handling](24-error-handling.md) for full details and examples. See [Lambdas](13-lambdas.md) for how block lambdas interact with `raise`.

---

## 6.6 When Blocks

`when` blocks are the concurrency primitive — they acquire cowns and execute a body. See [Concurrency](15-concurrency.md) for full details:

```verona
when (c1, c2) (ref1, ref2) ->
{
  *ref1 + *ref2
}
```

---

## 6.7 Else on Expressions

The `else` keyword can follow an expression to handle the `nomatch` case. This is the mechanism underlying `for` loops and `match` expressions:

```verona
// Inside the for loop desugaring:
let elem = it.next() else { break }
```

If `it.next()` returns `nomatch`, the `else` branch executes. Otherwise, the value is bound to `elem` with the `nomatch` alternative stripped from the type.

The `else` branch can be a block or an expression:

```verona
let elem = it.next() else { break }     // block form
let result = match v { ... } else 0;   // expression form
```

---

## 6.8 Match Expressions

`match` tests a value against a sequence of patterns. Each pattern is a case lambda — type test arms bind the value if it matches the type, value test arms compare the value using `==`.

### Syntax

```verona
match expr { arm1; arm2; ... }
```

The `match` keyword is followed by an expression and a block of arms:

```verona
let result = match x { (n: i32) -> n + 1; } else (0);
```

### The `else` Clause

The `else` clause is **optional**. It provides a fallback when no arm matches:

```verona
let result = match x { (n: i32) -> n + 1; } else (0);
```

Without `else`, a non-exhaustive match returns `nomatch` when no arm matches. The result type includes `nomatch`:

```verona
let result = match x { (n: i32) -> n + 1; }
// result type: i32 | nomatch
```

You can strip `nomatch` later with `else` on the expression:

```verona
let result = match x { (n: i32) -> n + 1; } else (0);
// result type: i32
```

> **Note:** Exhaustiveness checking is not yet implemented. Even if all types are covered by arms, the compiler does not verify this. Use `else` to ensure a clean result type.

### Parentheses

Match expressions do not need to be wrapped in parentheses. However, parentheses can be used for grouping when needed (e.g., inside a larger expression):

```verona
// Both are valid:
let a = match x { (n: i32) -> n; } else (0);
let b = (match x { (n: i32) -> n; }) else (0);
```

### Type Test Arms

A type test arm binds the matched value to a typed parameter:

```verona
match x
{
  (n: i32) -> n + 1;
  (s: string) -> s.size;
}
else (0)
```

If `x` is an `i32`, the first arm matches and `n` is bound to the value. If `x` is a `string`, the second arm matches. If neither matches, the `else` fallback is used (or `nomatch` is returned if no `else`).

### Value Test Arms

A value test arm compares the matched value against an expression using `==`:

```verona
let v: i32 = 42;
let name = match v { (1) -> 10; (42) -> 20; (99) -> 30; } else (0);
// name is 20
```

Value test arms call the `==` method on the matched value's type. If the type has no `==` method, or if the argument types don't match, the arm simply fails to match (it does not cause an error).

### Mixed Arms

Type and value test arms can be mixed. Arms are tried in order — the first match wins:

```verona
let result = match v
{
  (7) -> 100;          // value test: does v == 7?
  (x: i32) -> x;       // type test: is v an i32?
}
else (0)
```

### How Match Works

Under the hood, `match` desugars to a chain of case lambdas with `nomatch` subtraction:

1. Each arm becomes a lambda that either returns a result or `nomatch`.
2. Type test arms use a `typetest` conditional — if the value matches the type, the arm body executes.
3. Value test arms use `TryCallDyn` to call `==` — if the method doesn't exist or the argument types don't match, the arm returns `nomatch` rather than crashing.
4. Arms are chained as an `else`-if sequence: each arm tries in order, and the first non-`nomatch` result is used.
5. If all arms return `nomatch`, the `else` fallback is used. If there is no `else`, `nomatch` is returned.

### Examples

**Simple type dispatch:**

```verona
let x: i32 | string = get_value();
let result = match x
{
  (n: i32) -> n + 1;
  (s: string) -> s.size;
}
else (0);
```

**Value-based dispatch:**

```verona
let code: i32 = get_code();
let msg = match code
{
  (0) -> 0;
  (1) -> 10;
  (2) -> 20;
}
else (99);
```

**Without `else` (result includes `nomatch`):**

```verona
let result = match x { (n: i32) -> n; }
// result type: i32 | nomatch — use else to strip nomatch
```
