# 24. Error Handling

[← Table of Contents](README.md) | [Previous: Grammar Summary](23-grammar-summary.md) | [Next: Compile-Time Execution →](25-compile-time-execution.md)

This chapter describes Verona's error handling mechanisms.

---

## 24.1 Overview

Verona does not have exceptions or `try`/`catch`. Instead, it provides:

- **`raise`** — non-local return from a block lambda to the enclosing function.
- **`nomatch`** — a sentinel type for signaling the absence of a result.
- **`else` on expressions** — handling the `nomatch` case in iterators and `for` loops.
- **Runtime errors** — fatal to the current behavior (not recoverable).

---

## 24.2 Raise

`raise` performs a non-local return from a block lambda back to the function that created the lambda. The raised value becomes the return value of the enclosing function.

### Syntax

```verona
raise expr
```

### Example: Early Exit from a Search

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

use find_first_mod

main(): i32
{
  find_first(10, 42, 42)              // returns 42
}
```

When `raise x` executes inside the lambda, control returns directly from `find_first` with value `x` — skipping the rest of the function body.

### How Raise Works

1. A lambda containing `raise` captures the **raise target** (the enclosing function's stack frame) at creation time.
2. When `raise` executes, it restores the captured target and performs a non-local return.
3. The raised value becomes the return value of the enclosing function.

### Rules

- `raise` can only appear inside a lambda body. Using it outside a lambda is a compile error.
- The raise target is captured at lambda creation time, not at call time.
- There is no way to "catch" a raise — it always returns from the enclosing function.

### Escape Semantics

The raise target is the enclosing function's stack frame, captured when the lambda is created. If the lambda **escapes** the enclosing function (e.g., returned as a value or stored in a data structure) and `raise` is called after the enclosing function has already returned, the result is a **runtime error** — the raise target's frame no longer exists.

```verona
// DANGEROUS: the lambda escapes make_checker
make_checker(target: i32): i32 -> i32
{
  let check = (x: i32): i32 -> {
    if x == target { raise x }       // raise targets make_checker's frame
    x
  }
  check                              // lambda escapes the function
}
```

In practice, `raise` is designed for immediately-invoked block lambdas (like error-checking helpers called within the same function), not for lambdas that outlive their creator. The compiler does not currently prevent lambda escape — it is the programmer's responsibility to ensure the raise target is still valid when `raise` executes.

### Example: Using Raise with Iteration

```verona
find_in_array(arr: array[i32], target: i32): i32
{
  let check = (x: i32) -> {
    if x == target { raise x }
  }

  for arr.values() elem ->
  {
    check(elem)
  }

  0                                    // not found
}
```

---

## 24.3 Signaling Failure with `nomatch`

For recoverable "not found" or "no result" cases, functions return `T | nomatch`:

```verona
find(arr: array[i32], target: i32): i32 | nomatch
{
  for arr.values() elem ->
  {
    if elem == target { return elem }
  }
  nomatch
}
```

`nomatch` is also the sentinel for failed match arms — when a `match` expression's type test or value test arm doesn't match, it returns `nomatch`, which the `else` fallback handles:

```verona
let result = match x { (n: i32) -> n + 1; } else (0);
// If x is not i32, nomatch flows to else → result is 0
```

See [Control Flow §6.8](06-control-flow.md) for the full match expression syntax.

The caller handles `nomatch` through iterators or the `else` mechanism:

```verona
// Using for to handle T | nomatch implicitly:
for arr.values() elem ->
{
  // elem is T, not T | nomatch — the for loop strips nomatch
}
```

See [Types §3.3](03-types.md) for union type discrimination patterns and [Control Flow §6.8](06-control-flow.md) for match expressions.

---

## 24.4 Else on Expressions

The `else` keyword after an expression handles the `nomatch` case:

```verona
let elem = it.next() else { break }
```

If `it.next()` returns `nomatch`, the `else` branch executes. Otherwise, the value is bound with `nomatch` stripped from the type. This is the mechanism underlying `for` loop desugaring.

See [Control Flow §6.7](06-control-flow.md) for details.

---

## 24.5 Runtime Errors

Runtime errors (type mismatches, out-of-bounds access, stack reference escapes) are **fatal to the current behavior**. There is no way to catch or recover from a runtime error within the behavior that caused it.

In the context of `when` blocks:
- If a behavior (the body of a `when` block) encounters a runtime error, that behavior terminates.
- The cowns held by the behavior are released so other behaviors can proceed.
- The result cown of the failed `when` block receives no value.

For synchronous code (not inside a `when` block), a runtime error terminates the program.

---

## 24.6 Composing Error Handling: Union Types + Raise + Match

Verona's error handling is built from three composable primitives — `raise`, union types, and `match`. Together they cover the same ground as exceptions or `Result<T, E>` in other languages, with different tradeoffs.

### Returning Errors as Values

For recoverable errors, define an error class and return a union type:

```verona
parse_error
{
  msg: string;
  pos: usize;
}

parse_int(s: string): i32 | parse_error
{
  // ... parsing logic ...
  // On success:
  result
  // On failure:
  parse_error("invalid digit", pos)
}
```

The caller must handle the union — the compiler won't let you use the result as a bare `i32` without discriminating:

```verona
let result = match parse_int(input)
{
  (n: i32) -> n;
  (e: parse_error) -> { :::printval(e.msg); 0 }
}
```

### Early Exit with `raise`

`raise` provides non-local return — the equivalent of `throw` but without unwinding overhead or catch blocks. The raise target is the enclosing function:

```verona
process_all(items: array[string]): i32 | parse_error
{
  let check = (s: string) ->
  {
    match parse_int(s)
    {
      (n: i32) -> n;
      (e: parse_error) -> { raise e }  // bail out of process_all immediately
    }
  }

  var sum: i32 = 0;
  for items.values() item ->
  {
    sum = sum + check(item)
  }
  sum
}
```

When `raise e` executes, `process_all` returns immediately with the `parse_error` value. No cleanup blocks are needed — frame-local regions are freed automatically.

### Chaining Fallible Operations

Use `else` for one level of fallback, and `raise` for multi-step pipelines:

```verona
// Simple: one fallback
let x = parse_int(input) else { 0 }

// Pipeline: bail on first error
process_pipeline(a: string, b: string): i32 | parse_error
{
  let bail = (r: i32 | parse_error) ->
  {
    match r
    {
      (e: parse_error) -> { raise e }
      (n: i32) -> n;
    }
  }

  let x = bail(parse_int(a));
  let y = bail(parse_int(b));
  x + y
}
```

The `bail` lambda acts like Rust's `?` operator — it unwraps the success case or raises the error case.

### Error Hierarchies with Union Types

For richer error models, use wider union types:

```verona
io_error { msg: string; }
format_error { msg: string; line: usize; }

read_config(path: string): config | io_error | format_error
{
  // ... returns whichever error applies
}

main(): i32
{
  match read_config("app.conf")
  {
    (c: config) -> { 0 }
    (e: io_error) -> { :::printval(e.msg); 1 }
    (e: format_error) -> { :::printval(e.msg); 2 }
  }
}
```

Union types are open — you can always add more error variants without changing a base class or sealed hierarchy. The tradeoff: there is no exhaustiveness checking yet, so `match` without `else` includes `nomatch` in the result type.

### Summary of Error Patterns

| Pattern | When to Use |
|---------|-------------|
| `T \| nomatch` + `else` | Simple "not found" / "no result" cases |
| `T \| MyError` + `match` | Recoverable errors with context |
| `raise` inside a lambda | Early exit from loops or multi-step validation |
| `raise` + union return type | Pipeline of fallible steps — bail on first error |
| `cown[T \| MyError]` | Error propagation in concurrent behaviors |

---

## 24.7 Process Exit Codes

The return value of `main()` becomes the process exit code:

```verona
main(): i32
{
  0                                    // exit code 0 = success
}
```

This is the primary mechanism for reporting success or failure from a program.
