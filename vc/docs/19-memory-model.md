# 19. Memory Model

[← Table of Contents](README.md) | [Previous: Type Inference](18-type-inference.md) | [Next: Compiler Pipeline →](20-compiler-pipeline.md)

This chapter describes Verona's memory and ownership model.

---

## 19.1 Overview

Verona uses **region-based memory management** with reference counting. Objects are heap-allocated and organized into regions. A region is a group of objects with a single entry point — the region's root. When the root becomes unreachable, the entire region is collected.

There is no tracing garbage collector. Memory is reclaimed deterministically through reference counting and region deallocation.

---

## 19.2 Object Allocation

Every `new { ... }` expression allocates a new object on the heap. Objects carry a header with:
- A **type ID** identifying the object's class.
- A **location tag** encoding where the object lives (frame-local, in a region, immutable).
- A **reference count** tracking how many other objects and stack variables point to it.

```verona
let p = point(1, 2);                 // allocates a point object on the heap
```

---

## 19.3 Regions

A **region** is a collection of objects managed together. Each region has:
- A root object that serves as its entry point.
- A parent, which can be another region, a cown, a frame-local owner, or nothing.
- A **stack reference count** that tracks how many stack variables point into the region.

When a region's stack reference count drops to zero and it has no parent, the region and all its objects are deallocated.

### Region Types

The runtime supports two region strategies:

| Strategy | Description |
|----------|-------------|
| **Reference-counted** | Each object in the region tracks individual references. When an object's RC reaches zero, it is freed. |
| **Arena** | Internal reference counting is disabled. All objects in the region are freed together when the region is collected. |

---

## 19.4 Binding Semantics: `let` vs `var`

`let` and `var` control the **binding**, not the object:

| Binding | Reassignable? | Object mutable? |
|---------|---------------|-----------------|
| `let` | No — single assignment | Yes |
| `var` | Yes — can be reassigned | Yes |

Both `let` and `var` create references to objects. The object itself is always mutable — you can write to fields of a `let`-bound object:

```verona
let p = point(1, 2);
p.x = 10;                            // allowed — mutates the object, not the binding
```

The distinction is about the **name**, not the **value**:

```verona
var q = point(1, 2);
q = point(3, 4);                     // allowed — reassigns the binding

let r = point(1, 2);
// r = point(3, 4);                  // NOT allowed — let is single-assignment
```

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

### How Field Writes Work

1. Every class field automatically generates a `ref` accessor method:
   ```verona
   // For a field `x: i32` in class `point`, the compiler generates:
   ref x(self: point): ref[i32] { :::fieldref(self, x) }
   ```

2. When you write `obj.x = val`:
   - The compiler calls the `ref x` accessor, which returns a `ref[i32]`.
   - The compiler emits a `Store` instruction that writes `val` through the reference.
   - The store performs a region-aware exchange — it checks ownership rules, updates reference counts, and sets parent pointers as needed.

3. When you read `obj.x`:
   - If a non-ref `x` accessor exists, it is called.
   - Otherwise, the compiler auto-generates one that calls the `ref x` method and loads the value.

### `ref[T]` Is Not User-Constructible

You cannot create `ref[T]` values directly. They are produced only by `ref` methods (field accessors, `ref apply` on arrays). Holding a `ref[T]` beyond the statement that produced it is not supported — references are transient.

---

## 19.7 Freezing (Immutability)

Objects can be **frozen** — converted from mutable to immutable. A frozen object's region becomes immortal: no reference counting is needed, and the objects live until the program ends.

Freezing requires the region to be **sendable** — it must have no parent and exactly one stack reference. This ensures no other code holds a mutable reference to the region.

> **Note:** Freezing is a runtime concept. There is currently no syntax-level `freeze` operation exposed to user code. It is used internally by the runtime for cowns.

---

## 19.8 Cowns and Ownership Transfer

When a value is placed into a `cown` (via a `when` block), ownership of the value's region transfers to the cown. The region's parent is set to the cown, and the stack reference count is adjusted.

Inside a `when` block, the cown's content is temporarily accessible via the bound parameter. When the `when` block completes, the result value is placed into the result cown, and ownership is again transferred.

See [Concurrency](15-concurrency.md) for the user-facing semantics of cowns.
