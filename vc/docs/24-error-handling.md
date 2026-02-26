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
  };
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

### Example: Using Raise with Iteration

```verona
find_in_array(arr: array[i32], target: i32): i32
{
  let check = (x: i32) -> {
    if x == target { raise x }
  };

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

The caller handles `nomatch` through iterators or the `else` mechanism:

```verona
// Using for to handle T | nomatch implicitly:
for arr.values() elem ->
{
  // elem is T, not T | nomatch — the for loop strips nomatch
}
```

See [Types §3.3](03-types.md) for union type discrimination patterns.

---

## 24.4 Else on Expressions

The `else` keyword after an expression handles the `nomatch` case:

```verona
let elem = it.next() else { break };
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

## 24.6 Process Exit Codes

The return value of `main()` becomes the process exit code:

```verona
main(): i32
{
  0                                    // exit code 0 = success
}
```

This is the primary mechanism for reporting success or failure from a program.
