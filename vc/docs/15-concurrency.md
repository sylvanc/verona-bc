# 15. Concurrency (Cowns and When)

[← Table of Contents](README.md) | [Previous: Partial Application](14-partial-application.md) | [Next: Modules →](16-modules.md)

This chapter covers Verona's concurrency model based on concurrent ownership (cowns) and `when` blocks.

---

## 15.1 Cowns

A `cown[T]` (concurrent owner) wraps a value for safe concurrent access. Cowns are the unit of concurrency in Verona — all concurrent access to shared state goes through cowns.

Key properties:
- Access to a cown's contents is serialized — only one `when` block can access a cown at a time.
- Cowns can be shared between threads without data races.
- The contained value cannot be accessed directly — only through `when` blocks.
- Cowns are the **only** way to share mutable state between concurrent tasks.

---

## 15.2 Creating Cowns

Cowns are created by `when` blocks with empty cown and parameter lists:

```verona
let c = when () () -> { 42 };
```

This creates a new `cown[i32]` holding the value `42`. A `when` block with no cowns runs immediately (it has no dependencies to wait for) and wraps its result in a cown.

There is no `cown::create()` function — `when` is the only way to create cowns.

---

## 15.3 When Blocks

`when` acquires one or more cowns, binds exclusive references, and executes a body:

```verona
when (c) (x) ->
{
  let v = (*x).f;
  v + v
}
```

### Syntax

```verona
when (cown1, cown2, ...) (ref1, ref2, ...) ->
{
  body
}
```

- The first tuple lists the cowns to acquire.
- The second tuple names the parameters for the exclusive references.
- The number of cowns must match the number of parameters.
- Inside the body, `*ref` dereferences the cown reference to access the held value.

### Scheduling

A `when` block is scheduled as a **behavior** — an asynchronous unit of work. The runtime scheduler:

1. Waits until **all** requested cowns are available (not held by another behavior).
2. Acquires exclusive access to all requested cowns atomically.
3. Runs the behavior body on a scheduler thread.
4. Releases all cowns when the body completes.

Behaviors are non-blocking from the caller's perspective — `when` returns immediately with a result cown. The behavior runs later when its cowns become available.

### Return Value

A `when` block returns a new `cown` holding the result of the body:

```verona
let a = when () () -> { cell(10) };            // cown[cell]
let b = when () () -> { cell(20) };            // cown[cell]

// Combine two cowns — result is cown[cell]
let c = when (a, b) (x, y) ->
{
  let sum = (*x).f + (*y).f;
  cell(sum)
}
```

The result cown is created immediately (so it can be used as a dependency for later `when` blocks), but its value is populated only when the behavior completes.

### Deadlock Freedom

Acquiring multiple cowns in a single `when` block is **deadlock-free** — the runtime handles the ordering. You should always acquire all needed cowns in one `when` rather than nesting `when` blocks:

```verona
// Good: acquire both at once
let d = when (b, c) (y, z) ->
{
  (*y).f + (*z).f
}

// Bad: nested when blocks can deadlock
// (don't do this)
```

---

## 15.4 Read Access

The `read` method provides **shared read-only access** to a cown:

```verona
c.read
```

Multiple readers can access a cown simultaneously, but read access is exclusive with write access. When a cown is accessed via `read` in a `when` block, the behavior receives a read-only reference — writes through it are not permitted.

### Example

```verona
let c = when () () -> { cell(42) }

// Read-only access — multiple readers can run concurrently
let r = when (c.read) (x) ->
{
  (*x).f                              // read OK
  // (*x).f = 10;                     // NOT allowed — read-only reference
}
```

Using `c.read` instead of `c` tells the scheduler this behavior only needs read access, enabling concurrent execution with other read-only behaviors on the same cown.

---

## 15.5 Dereference (`*`)

Inside a `when` block, `*ref` dereferences a cown reference to access the underlying value:

```verona
when (c) (x) ->
{
  (*x).field                          // access field of the cown's value
}
```

The `*` operator is defined as a `ref` method on `ref[T]`, returning a `ref[T]` (allowing both reading and writing through the reference).

---

## 15.6 Cown Lifecycle

Cowns are reference-counted. When no references to a cown remain (no variables, no pending behaviors), the cown and its contents are deallocated.

When a value is placed into a cown, ownership of the value's region transfers to the cown. See [Memory Model](19-memory-model.md).

---

## 15.7 Complete Example

```verona
use
{
  printval = "printval"(any): none;
}

cell
{
  f: i32;
}

main(): i32
{
  // Create cowns
  let a = when () () -> { cell(10) }
  let b = when () () -> { cell(20) }

  // Combine cowns
  let c = when (a, b) (x, y) ->
  {
    let sum = (*x).f + (*y).f;
    cell(sum)
  }

  // Use the result
  let d = when (c) (z) ->
  {
    :::printval((*z).f);
    (*z).f
  }

  0
}
```

---

## 15.8 Runtime Errors in Behaviors

If a behavior (the body of a `when` block) encounters a runtime error (type mismatch, out-of-bounds access, stack reference escape), the error is **fatal to that behavior**:

- The behavior terminates immediately.
- All interpreter frames for the behavior are torn down (registers, stack allocations, finalizers, and frame-local regions are cleaned up).
- The cowns held by the behavior are released so other behaviors can proceed.
- The result cown of the failed `when` block receives no value.

There is no way to catch or recover from a runtime error within the behavior that caused it. This is by design — runtime errors indicate logic bugs, not expected failure modes. For expected failure cases, use `nomatch` return values or `raise` for non-local return. See [Error Handling](24-error-handling.md).
