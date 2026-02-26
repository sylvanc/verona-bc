# 6. Control Flow

[← Table of Contents](README.md) | [Previous: Expressions](05-expressions.md) | [Next: Functions →](07-functions.md)

This chapter covers conditional expressions, loops, and early exit.

---

## 6.1 If / Else

`if` is an expression — it evaluates to a value:

```verona
let x = if condition { 1 } else { 0 };
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
if false { i32 0 }
else if false { i32 1 }
else (i32 2)
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
    let elem = it.next() else { break };
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
  if skip_condition { continue };
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
  };
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

The `else` keyword can follow an expression to handle the `nomatch` case. This is the mechanism underlying the `for` loop:

```verona
// Inside the for loop desugaring:
let elem = it.next() else { break };
```

If `it.next()` returns `nomatch`, the `else` branch executes. Otherwise, the value is bound to `elem` with the `nomatch` alternative stripped from the type.

> **Note:** The `else` on expression mechanism is primarily used in the `for` loop desugaring. It is not currently a general-purpose user-facing feature for arbitrary union type discrimination. For handling `T | nomatch`, use `for` loops or design APIs around iterators.
