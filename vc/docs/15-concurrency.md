# 15. Concurrency (Cowns and When)

[← Table of Contents](README.md) | [Previous: Partial Application](14-partial-application.md) | [Next: Modules →](16-modules.md)

This chapter covers Verona's concurrency model based on concurrent ownership (cowns) and `when` blocks.

---

## 15.1 Why Cowns?

Concurrent programming is hard because shared mutable state creates data races, deadlocks, and subtle bugs. Most languages address this with locks, channels, or `async`/`await` — all of which put the burden on the programmer to get synchronization right.

Verona takes a different approach: **you cannot share mutable state at all**. Instead, mutable state is wrapped in `cown[T]` (concurrent owner) values, which enforce exclusive access at runtime. The result is:

- **No data races** — only one `when` block accesses a cown at a time.
- **No deadlocks** — multi-cown `when` blocks acquire all cowns atomically.
- **No manual locking** — the scheduler handles all synchronization.
- **No shared-nothing message passing** — cowns hold mutable state directly, avoiding serialization/deserialization overhead.

> Think of a `cown` as a mutex and its protected data fused into a single value, where the language prevents you from accessing the data without holding the lock.

---

## 15.2 Cowns

A `cown[T]` (concurrent owner) wraps a value for safe concurrent access. Cowns are the unit of concurrency in Verona — all concurrent access to shared state goes through cowns.

Key properties:
- Access to a cown's contents is serialized — only one `when` block can access a cown at a time.
- Cowns can be shared between threads without data races.
- The contained value cannot be accessed directly — only through `when` blocks.
- Cowns are the **only** way to share mutable state between concurrent tasks.

---

## 15.3 Creating Cowns

Cowns are created by `when` blocks. A `when` block is an **asynchronous behavior** — it schedules work that runs later and returns a `cown` holding the eventual result. The returned cown becomes "available" (usable by another `when`) once the behavior completes:

```verona
let c = when () () -> { 42 };
```

This creates a new `cown[i32]` holding the value `42`. A `when` block with no cown arguments (`()`) is schedulable immediately (no dependencies) — but the behavior still runs asynchronously on a scheduler thread.

A convenience wrapper `cown[T]::create(val)` also exists for creating a cown directly from a value without writing a `when` block.

---

## 15.4 When Blocks

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

A `when` block is an **asynchronous behavior** — a unit of work that runs independently. The `when` expression returns a result cown immediately; the behavior body always runs later on a scheduler thread, even for `when` blocks with no dependencies. The runtime scheduler:

1. Waits until **all** requested cowns are available (not held by another behavior).
2. Acquires exclusive access to all requested cowns atomically.
3. Runs the behavior body on a scheduler thread.
4. Releases all cowns when the body completes.
5. Populates the result cown with the body's return value.

Behaviors are non-blocking from the caller's perspective — `when` returns immediately. The behavior runs concurrently when its cowns become available.

### Return Value

Every `when` block returns a new `cown` wrapping the result of the body. This is how cowns are created — the only way to get a new cown is through `when`:

```verona
let a = when () () -> { cell(10) };            // cown[cell] — created immediately
let b = when () () -> { cell(20) };            // cown[cell] — created immediately

// Combine two cowns — result is cown[cell]
let c = when (a, b) (x, y) ->
{
  let sum = (*x).f + (*y).f;
  cell(sum)
}
```

The result cown is created immediately (so it can be used as a dependency for later `when` blocks), but its value is populated only when the behavior completes. This means you can chain `when` blocks by passing result cowns as dependencies — the scheduler handles ordering automatically.

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

## 15.5 Read Access

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

## 15.6 Dereference (`*`)

Inside a `when` block, `*ref` dereferences a cown reference to access the underlying value:

```verona
when (c) (x) ->
{
  (*x).field                          // access field of the cown's value
}
```

The `*` operator is defined as a `ref` method on `ref[T]`, returning a `ref[T]` (allowing both reading and writing through the reference).

---

## 15.7 Cown Lifecycle

Cowns are reference-counted. When no references to a cown remain (no variables, no pending behaviors), the cown and its contents are deallocated.

When a value is placed into a cown, ownership of the value's region transfers to the cown. See [Memory Model](19-memory-model.md).

---

## 15.8 Complete Example

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

## 15.9 Runtime Errors in Behaviors

If a behavior (the body of a `when` block) encounters a runtime error (type mismatch, out-of-bounds access, stack reference escape), the error is **fatal to that behavior**:

- The behavior terminates immediately.
- All interpreter frames for the behavior are torn down (registers, stack allocations, finalizers, and frame-local regions are cleaned up).
- The cowns held by the behavior are released so other behaviors can proceed.
- The result cown of the failed `when` block receives no value.

There is no way to catch or recover from a runtime error within the behavior that caused it. This is by design — runtime errors indicate logic bugs, not expected failure modes. For expected failure cases, use `nomatch` return values or `raise` for non-local return. See [Error Handling](24-error-handling.md).

---

## 15.10 Concurrency FAQ

### How does `main()` interact with `when` blocks?

The interpreter waits for all pending behaviors to complete before exiting. This means behaviors scheduled from `main()` will run even though `main()` returns immediately:

```verona
main(): i32
{
  let c = when () () -> { cell(42) };
  let d = when (c) (x) -> { (*x).f };
  0                                    // returns immediately, but behaviors run to completion
}
```

### How do I handle errors in cowns?

There are no "failed" cowns. A behavior always produces a result — if something might go wrong, model it with a **union type**:

```verona
// The behavior returns i32 | none — never "fails"
let result = when (input) (x) ->
{
  if (*x).valid
  {
    (*x).compute
  }
  else
  {
    none::create()                    // signal failure as a value
  }
};

// Downstream behavior handles the union
let final = when (result) (r) ->
{
  match *r { (n: i32) -> n; } else (0)
};
```

For richer errors, use a custom error class in the union: `cown[result | my_error]`. The behavior returns whichever variant applies, and downstream `when` blocks discriminate with `match`.

### Can I get a value out of a cown synchronously?

No. Cown contents are only accessible inside `when` blocks. This is fundamental to the concurrency model — direct access would break the serialization guarantee. If you need a value for the process exit code, have the final `when` block write to a shared state or use `:::printval` for output.

### How do external resources interact with the scheduler?

The scheduler tracks external resources — things outside Verona's control (file descriptors, network connections, OS callbacks). It will not shut down while external resources remain:

- `ffi::external.add` increments the count (method on the `external` singleton).
- `ffi::external.remove` decrements the count.
- `ffi::register_external_notify(lambda)` registers a lambda that fires on each add/remove event.

See [FFI §17.8](17-ffi.md) for details.

### How do I chain cowns in a pipeline?

Pass result cowns as dependencies to later `when` blocks. The scheduler automatically orders them:

```verona
cell { f: i32; }

main(): i32
{
  // Step 1: produce initial data
  let step1 = when () () -> { cell(10) };

  // Step 2: depends on step1 — doubles the value
  let step2 = when (step1) (x) ->
  {
    cell((*x).f + (*x).f)
  };

  // Step 3: depends on step2 — adds 2
  let step3 = when (step2) (y) ->
  {
    cell((*y).f + 2)
  };

  // Step 4: combine step1 and step3
  let final = when (step1, step3) (a, b) ->
  {
    cell((*a).f + (*b).f)
  };

  0
}
```

Each `when` block returns its own `cown`, which the next step uses as a dependency. The runtime handles all ordering — step 2 waits for step 1, step 3 waits for step 2, and the final step waits for both step 1 and step 3. This is the Verona equivalent of an `async` pipeline.

### What about fan-out and fan-in?

Create multiple independent cowns (fan-out), then combine them in a single `when` block (fan-in):

```verona
// Fan out: a and b run concurrently (no shared dependencies)
let a = when () () -> { cell(compute_a()) };
let b = when () () -> { cell(compute_b()) };

// Fan in: waits for both a and b
let result = when (a, b) (x, y) ->
{
  cell((*x).f + (*y).f)
};
```
