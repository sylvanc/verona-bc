# 11. Tuples and Destructuring

[← Table of Contents](README.md) | [Previous: Generics](10-generics.md) | [Next: Arrays →](12-arrays.md)

This chapter covers tuple construction, destructuring, and related patterns.

---

## 11.1 Tuple Construction

Tuples are created with parenthesized, comma-separated expressions:

```verona
let pair = (i32 3, i32 5);
let triple = (i32 1, i32 2, i32 3);
```

### Heterogeneous Tuples

Tuple elements can have different types:

```verona
let mixed = (i32 3, i64 5);          // (i32, i64)
let data = (i32 42, "hello", true);  // (i32, string, bool)
```

Each element's type is tracked individually through the compiler pipeline.

---

## 11.2 Tuple Destructuring

Tuples can be destructured into individual bindings:

```verona
let t = (i32 3, i32 5);
(let a, let b) = t;                  // a = 3, b = 5
```

### Mixing `let`, `var`, and `_`

You can freely mix binding types and discards:

```verona
let t = (i32 3, i32 5);

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
let t = (i32 3, i32 5);
(x, y) = t;                          // x = 3, y = 5
```

### Mixed Assignment and Binding

```verona
var x: i32 = 0;
let t = (i32 3, i32 5);
(x, let b) = t;                      // assign to existing x, create new b
```

---

## 11.3 Tuple Return Values

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

## 11.4 Tuples in For Loops

For loop parameters can destructure tuples when the iterator yields them:

```verona
for iter (key, value) ->
{
  // body
}
```
