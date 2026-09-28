# 19. Memory Model

[← Table of Contents](README.md) | [Previous: Type Inference](18-type-inference.md) | [Next: Compiler Pipeline →](20-compiler-pipeline.md)

This chapter describes Verona's memory and ownership model — how objects are allocated, where they live, and what rules govern references between them.

---

## 19.1 Overview

Verona uses **region-based memory management** with reference counting. There is no tracing garbage collector. Memory is reclaimed deterministically through reference counting and region deallocation.

Objects are organized into **regions**. A region is a group of objects managed together, with a single entry point (its root). When the root becomes unreachable, the entire region and all its objects are deallocated.

---

## 19.2 Allocation Locations

Every object lives in one of three locations:

| Location | Description | Lifetime |
|----------|-------------|----------|
| **Stack** | Fast, scoped allocation | Owning logical frame |
| **Frame-local region** | Default heap allocation | Until dragged or frame exits |
| **Heap region** | Explicit region allocation | Until region root becomes unreachable |

### How Allocation Location Is Determined

The programmer does not choose where objects are allocated — the runtime decides:

- **Primitives** (`i32`, `bool`, etc.) and small values are typically stack-allocated.
- **`new` expressions** allocate objects in the current function's **frame-local region** (a temporary heap region).
- **Heap regions** are created when a value is placed into a cown (via `when`), or when an object is dragged into another region.

You don't need to annotate allocation locations. The rules are simple: `new` goes to the frame-local region, cowns create heap regions, and the runtime drags objects between regions as needed.

### Stack Allocation

Stack objects and arrays have logical-frame lifetime. They are destroyed when
their owning frame returns, unwinds after a raise, or is reused for a tailcall.
The native runtime keeps them in stable-address, runtime-managed storage; they
do not rely on the native C++ call stack for lifetime management.

A value allocated by the current frame cannot be returned, raised past that
frame, or passed through tailcall frame reuse. A callee may use, return, or
raise a stack value owned by a surviving ancestor frame because the owner
remains active. Violations are reported as `bad stack escape`.

### Frame-Local Regions

Every function call has its own **frame-local region** — a temporary heap region that holds objects created during the call. This is the default allocation target for `new`. When the function returns:

- If the return value is frame-local, it is **dragged** (moved) to the caller's frame-local region.
- All remaining frame-local objects are freed.

Frame-local regions do not participate in stack reference counting — they are managed by the call stack.

### Heap Regions

Heap regions are explicitly created and persist until collected. Each heap region has:
- A root object as its entry point.
- A **parent**, which can be another region, a cown, or nothing.
- A **stack reference count** tracking how many registers point into the region.

When a heap region's stack reference count drops to zero and it has no parent, the region and all its contents are freed.

---

## 19.3 Region Dragging

**Dragging** is the automatic process of moving objects from one region to another. It happens transparently — the programmer does not need to manage it. Dragging occurs in several situations:

### Storing into a Field

When you assign a frame-local object into a field of an object in a different region, the frame-local object (and everything it references) is dragged into the target region:

```verona
let container = cell(none::create());
let value = myobj(42);               // frame-local
container.inner = value;             // value is dragged into container's region
```

### Returning a Value

When a function returns a frame-local object, it is dragged to the caller's frame-local region:

```verona
make(): cell
{
  let c = cell(42);                  // frame-local in make's region
  c                                  // dragged to caller's region on return
}
```

### Placing into a Cown

When a frame-local object is placed into a cown (via `when`), it is dragged into a fresh heap region owned by the cown.

### What Cannot Be Dragged

- **Stack objects** cannot be dragged into any region. They may cross a call or
  raise boundary only when their owning ancestor frame survives. A
  current-frame stack value cannot cross return, raise, or tailcall reuse.
- **Region objects with existing parents** — a region can only have one owner. Attempting to place an already-owned region into another is an error.

---

## 19.4 Binding Semantics: `let` vs `var`

`let` and `var` control the **binding**, not the object:

| Binding | Reassignable? | Object mutable? |
|---------|---------------|-----------------|
| `let` | No — single assignment | Yes |
| `var` | Yes — can be reassigned | Yes |

Both `let` and `var` create **aliases** to objects — not copies or moves. The object itself is always mutable through any alias:

```verona
let a = point(1, 2);
let b = a;                            // b aliases the same object
b.x = 10;                            // a.x is now also 10
```

Assignment never copies, never moves, never invalidates the source binding. Two names for the same object see each other's mutations.

The `let`/`var` distinction is about the **name**, not the **value**:

```verona
var q = point(1, 2);
q = point(3, 4);                     // allowed — reassigns the binding

let r = point(1, 2);
// r = point(3, 4);                  // NOT allowed — let is single-assignment
```

> **Aliasing and concurrency:** Because assignment aliases, two aliases to the same object can both mutate it. Within a single thread, this is intentional — Verona is not a pure language. Across threads, shared mutable state must go through [cowns](15-concurrency.md), which serialize access and prevent data races.

---

## 19.5 Assignment Returns the Previous Value

Assignment (`=`) is an expression that evaluates to the **previous value** of the left-hand side:

```verona
var x = 1;
let old = (x = 2);                   // old is 1, x is now 2
```

For field writes through `ref[T]`, the same applies — the exchange returns the old value at that location.

---

## 19.6 References and `ref[T]`

`ref[T]` is a mutable reference wrapper used for field and array element writes. It is not a general-purpose pointer — it exists specifically to enable assignment syntax.

### How Field Assignment Works

When you write `obj.field = val`, three things happen:

1. **The compiler calls the `ref` accessor.** Every class field automatically generates a `ref` method:
   ```verona
   // For a field `x: i32` in class `point`, the compiler generates:
   ref x(self: point): ref[i32] { :::fieldref(self, x) }
   ```
   This returns a `ref[i32]` pointing into the object.

2. **The compiler emits a Store instruction.** The store writes `val` through the reference, performing a region-aware exchange — it checks ownership rules, updates reference counts, and sets parent pointers as needed.

3. **The old value is returned.** Assignment is an exchange — it returns the previous value (see §19.5).

When you read `obj.field`, the compiler either calls a non-ref accessor (if one exists) or auto-generates one that loads through the ref.

### `ref[T]` Is Not User-Constructible

You cannot create `ref[T]` values directly. They are produced only by `ref` methods (field accessors, `ref apply` on arrays).

### `ref[T]` and Regions

A `ref[T]` points into the same region as the object it was obtained from. Using a `ref[T]` within that region is safe. What you **cannot** do:

- **Return a `ref[T]` into the current frame's stack storage** — the referenced
  storage is destroyed when that frame returns. A reference into a surviving
  ancestor frame remains valid.
- **Return, raise, or tailcall with a current-frame stack value** — runtime
  error (`bad stack escape`).
- **Store a stack value into a region or into older stack storage** — the value
  could outlive its owning frame. This is caught at runtime (`bad store`).
- **Return a frame-local value that contains a reference to a stack value** — the contained reference becomes dangling. This is caught at runtime.

The compiler generates code that stores through or loads from references immediately. You should not store a `ref[T]` in a `let` or `var` for later use — the compiler's generated code handles references transiently.

```verona
let val = obj.field;                  // loads through ref, binds the value
obj.field = new_val;                  // stores through ref in one statement
```

---

## 19.7 Region Strategies

The runtime supports two region strategies:

| Strategy | Description |
|----------|-------------|
| **Reference-counted** | Each object in the region tracks individual references. When an object's internal RC reaches zero, it is freed immediately. |
| **Arena** | Internal reference counting is disabled. All objects are freed together when the region is collected. Faster allocation, no per-object overhead. |

Both strategies use the same **stack reference count** at the region level to determine when the region itself can be freed.

---

## 19.8 Runtime Errors

The runtime detects several region and reference errors at runtime:

| Error | Message | Cause |
|-------|---------|-------|
| `bad stack escape` | Returning a stack-allocated value from a function, or raising past its frame. |
| `bad store` | Region invariant violation: storing causes a cycle, double parent, or frame-local→stack reference. Storing a read-only value. |
| `bad store target` | Writing to an immutable or read-only reference. RegisterRef lifetime violation. |
| `bad type` | Subtype check failed on call argument, return value, field store, or array store. |
| `bad array index` | Array index out of bounds. |
| `bad args` | Wrong number of arguments to a function or constructor. |
| `bad freeze` | Freezing a stack-allocated value or an arena-managed region. |

These errors are **fatal** — the current function (or behavior, in a `when` block) terminates immediately. They indicate logic bugs, not expected failure modes.

---

## 19.9 Freezing (Immutability)

`freeze(value)` returns the same value after making its reachable managed graph
immutable. Primitive values are copied unchanged. Freezing an already immutable
or immortal value is a no-op. `let` still constrains only a binding; `freeze`
changes the managed objects themselves.

The runtime discovers strongly connected components (SCCs) in the reachable
graph. Every published SCC has one representative with an atomic reference
count (ARC); other members point directly to that representative. References
within an SCC do not keep it alive. References from registers, mutable objects,
and other immutable SCCs contribute to the representative's ARC. When it
reaches zero, the collector reclaims the whole component.

Freezing may select only the reachable part of an RC region. Unreached mutable
objects remain in that region, and their references into the frozen graph are
included in ARC accounting. Reachable frame-local objects and heap subregions
are also handled. The native runtime rejects stack-allocated roots and arena
regions with `bad freeze`: arena objects deliberately omit the per-object
counts needed to derive correct SCC reference counts.

Writes into immutable storage raise `bad store target`. Reads need no locks;
only changes to the published SCC's ARC are atomic.

---

## 19.10 Cowns and Ownership Transfer

When a value is placed into a `cown` (via a `when` block), ownership of the value's region transfers to the cown. The region's parent is set to the cown, and the stack reference count is adjusted.

Inside a `when` block, the cown's content is temporarily accessible via the bound parameter. When the `when` block completes, the result value is placed into the result cown, and ownership is again transferred.

See [Concurrency](15-concurrency.md) for the user-facing semantics of cowns.

---

## 19.11 Object Teardown

When a region is collected or a frame is unwound, objects are torn down:
1. A user-defined `final(self: T)` method runs, if present.
2. Managed fields are dropped, releasing outgoing references.
3. Object storage is freed.

Collection is two-phase: all queued objects finish steps 1 and 2 before any of
their storage is released. This lets a finalizer inspect another object that is
being collected in the same batch. A finalizer receives borrowed, read-only
`self`; it cannot resurrect the object or prevent collection.

If a native finalizer raises a runtime error, the runtime reports that error,
restores the caller's cleanup boundary, and continues dropping fields and
releasing storage. This boundary contains cleanup failures; it is not a
source-level exception handler.

---

## 19.12 What's Being Designed

Several memory model features are actively being designed:

### Explicit Region Syntax

Currently, regions are created implicitly (frame-local regions per function call, heap regions via cowns). The planned feature will allow programmers to explicitly create and manage regions, enabling patterns like sendable subgraphs and region transfer between cowns.

### Compile-Time Region Safety

Most region violations (stack escape, invalid stores, lifetime errors) are
caught at **runtime**. The compiler already rejects cases determined entirely
by static class layout, such as creating a region whose entry-point class is
an empty singleton. Planned analysis will move more checks to compile time,
giving programmers earlier feedback.
