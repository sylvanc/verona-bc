# 11. Tuples and Destructuring

[← Table of Contents](README.md) | [Previous: Generics](10-generics.md) | [Next: Arrays →](12-arrays.md)

This chapter covers tuple construction, destructuring, splat patterns, and related patterns.

---

## 11.1 Tuple Construction

Tuples are created with parenthesized, comma-separated expressions:

```verona
let pair = (3, 5);
let triple = (1, 2, 3);
```

### Heterogeneous Tuples

Tuple elements can have different types:

```verona
let mixed = (i32 3, i64 5);          // (i32, i64)
let data = (42, "hello", true);      // (u64, string, bool)
```

Each element's type is tracked individually through the compiler pipeline.

### Tuple Types

Tuple types are written as parenthesized, comma-separated types:

```verona
(i32, i32)                            // pair of i32
(i32, string, bool)                   // triple
```

There are no 0-tuples or 1-tuples. Use `none` for an empty value and a bare type for a single value.

---

## 11.2 Tuple Destructuring

Tuples can be destructured into individual bindings:

```verona
let t = (3, 5);
(let a, let b) = t;                  // a = 3, b = 5
```

### Mixing `let`, `var`, and `_`

You can freely mix binding types and discards:

```verona
let t = (3, 5);

(let a, let b) = t;                  // two new immutable bindings
(var x, let y) = t;                  // mutable x, immutable y
(_, let b) = t;                      // discard first, bind second
(let a, _) = t;                      // bind first, discard second
(_, _) = t;                          // discard everything
```

### Assignment to Existing Variables

Destructuring can assign to existing mutable variables:

```verona
var x: i32 = 0;
var y: i32 = 0;
let t = (3, 5);
(x, y) = t;                          // x = 3, y = 5
```

### Mixed Assignment and Binding

```verona
var x: i32 = 0;
let t = (3, 5);
(x, let b) = t;                      // assign to existing x, create new b
```

---

## 11.3 Splat Destructuring

When the number of tuple elements isn't known at the point of destructuring (e.g., in generic code), or when you want to capture multiple remaining elements at once, use **splat** syntax with `...`:

### Capturing Remaining Elements

```verona
let t = (1, 2, 3, 4, 5);
(let a, let rest...) = t;            // a = 1, rest = (2, 3, 4, 5)
```

The splat variable (`rest...`) captures all remaining elements. Its type depends on how many elements remain after the fixed bindings:

| Remaining elements | Type | Value |
|---|---|---|
| 0 | `none` | `none` |
| 1 | Scalar `T` | The single element |
| 2+ | `(T1, T2, ...)` | A new tuple |

### Splat Position

The splat can appear at any position — start, middle, or end:

```verona
let t = (1, 2, 3, 4);

// Splat at end
(let a, let rest...) = t;            // a = 1, rest = (2, 3, 4)

// Splat at start
(let rest..., let d) = t;            // rest = (1, 2, 3), d = 4

// Splat in middle
(let a, let mid..., let d) = t;      // a = 1, mid = (2, 3), d = 4
```

### Don't-Care Splat

Use `_...` to skip remaining elements without binding them:

```verona
let t = (1, 2, 3, 4, 5);
(let a, let b, _...) = t;            // a = 1, b = 2, rest discarded
```

This is useful when you only care about a few elements and want to ignore the rest, regardless of how many there are.

### Arity Checking

The compiler statically checks that the tuple has enough elements for the fixed bindings. If there are too few, it's a compile error:

```verona
let t = (1, 2);

// Error: tuple has 2 elements, but destructuring requires at least 3
(let a, let b, let c...) = t;        // error at compile time when types are known
```

Without a splat, exact-arity matching is required:

```verona
let t = (1, 2);
(let a, let b, let c) = t;           // error: tuple index 2 out of range
```

### Restrictions

- At most one splat per destructuring pattern.
- Splat lets cannot have type annotations: `let rest: T...` is an error.
- `...` cannot appear in type positions.

---

## 11.4 Tuple Return Values

Functions can return tuples:

```verona
divide(a: i32, b: i32): (i32, i32)
{
  (a / b, a % b)                     // returns (quotient, remainder)
}

let result = divide(17, 5);
(let q, let r) = result;             // q = 3, r = 2
```

---

## 11.5 Tuples in For Loops

For loop parameters can destructure tuples when the iterator yields them:

```verona
for iter (key, value) ->
{
  // key and value are bound from each tuple yielded by iter
}
```

### Example: Iterating Over Pairs

```verona
pair_iter
{
  index: usize;
  data: array[i32];

  next(self: pair_iter): (usize, i32) | nomatch
  {
    if self.index >= self.data.size { nomatch }
    else
    {
      let i = self.index = self.index + 1;
      (i, (self.data)(i))
    }
  }
}

main(): i32
{
  let arr = ::(i32 10, 20, 30);
  var sum = 0;

  for pair_iter(0, arr) (index, value) ->
  {
    sum = sum + value
  }

  sum                                  // 60
}
```

Each call to `next()` returns a `(usize, i32)` tuple, which the `for` loop destructures into `index` and `value`.
```
