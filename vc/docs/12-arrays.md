# 12. Arrays

[← Table of Contents](README.md) | [Previous: Tuples](11-tuples.md) | [Next: Lambdas →](13-lambdas.md)

This chapter covers array creation, indexing, iteration, and array literals.

---

## 12.1 Array Creation

Arrays are created using the `fill` static method:

```verona
// Default-filled: 10 elements, each initialized with T::create()
let a = array[i32]::fill(10);

// Value-filled: 3 elements, each initialized to 42
let b = array[i32]::fill(3, 42);
```

The size argument is a `usize`. The `from` parameter is optional and defaults to `T::create()` (the type's default value).

---

## 12.2 Array Literals

The `::()` syntax creates an array literal:

```verona
let c = ::(i32 5, 10, 15);           // array[i32] with 3 elements
```

### Type Inference for Literals

Array literal elements are type-inferred with **sibling refinement** — if any element has an explicit type, the others are refined to match:

```verona
let d = ::(i32 1, 2, 3);            // 2 and 3 are inferred as i32
```

The dominant non-default type is used to refine all default-typed literals in the array.

---

## 12.3 Indexing

Arrays are indexed using juxtaposition (calling `apply`):

```verona
let val = arr(i);                     // read: calls arr.apply(i)
arr(i) = val;                         // write: calls arr.ref apply(i), stores val
```

The index is a `usize`.

---

## 12.4 Iteration

The `values()` method returns an `arrayiter[T]` iterator:

```verona
for arr.values() elem ->
{
  // elem is bound to each element in order
}
```

The iterator's `next()` method returns `T | nomatch`. The `for` loop calls `next()` each iteration and stops when it returns `nomatch`.

---

## 12.5 Size

The `size` property returns the number of elements:

```verona
let n = arr.size;                     // n: usize
```

---

## 12.6 Complete Example

```verona
main(): i32
{
  // Create and fill an array
  let arr = array[i32]::fill(10);
  var index = 0;

  while index < arr.size
  {
    arr(index) = index.i32;
    index = index + 1
  }

  // Sum all elements
  var sum = 0;

  for arr.values() i ->
  {
    sum = sum + i
  }

  sum                                 // 45 (0 + 1 + ... + 9)
}
```

---

## 12.7 Array Methods Summary

| Method | Signature | Description |
|--------|-----------|-------------|
| `fill` | `fill(size: usize, from: T = T): array[T]` | Create array with given size |
| `apply` | `apply(self: array[T], index: usize): T` | Read element at index |
| `ref apply` | `ref apply(self: array[T], index: usize): ref[T]` | Write element at index |
| `size` | `size(self: array[T]): usize` | Number of elements |
| `values` | `values(self: array[T]): arrayiter[T]` | Get iterator |

---

## 12.8 Arrays Are Fixed-Size

Arrays in Verona are **fixed-size** — the size is set at creation time and cannot change. There is no `append`, `remove`, `push`, `pop`, `slice`, or `concat` on the built-in `array[T]`.

This is intentional. Verona has **no standard library** beyond `_builtin` (see [Program Structure §2.6](02-program-structure.md)). Growable lists, hash maps, sets, queues, and other collections are provided by **packages**, not built into the language. This keeps the language core minimal and suitable for environments ranging from embedded systems to application servers.

The built-in `array[T]` is the primitive fixed-size contiguous array that everything else builds on. If you need a dynamic collection, import a package that provides one.

### Out-of-Bounds Access

Array indexing is **bounds-checked at runtime**. Accessing an index `>= size` produces a fatal `bad array index` error:

```verona
let a = array[i32]::fill(3);
a(5) = 10;                            // runtime error: bad array index
```

See [Memory Model §19.8](19-memory-model.md) for the complete list of runtime errors.
