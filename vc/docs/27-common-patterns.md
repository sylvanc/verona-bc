# 27. Common Patterns

[← Table of Contents](README.md) | [Previous: Gotchas](26-gotchas.md)

This chapter collects idiomatic patterns and recipes for common programming tasks in Verona.

---

## 27.1 Bitmask Testing

Use bitmask patterns to report multiple test results through a single exit code:

```verona
main(): i32
{
  var result = 0;

  if !(test_addition()) { result = result + 1 }
  if !(test_subtraction()) { result = result + 2 }
  if !(test_comparison()) { result = result + 4 }

  result                              // 0 = all passed
}
```

Each test gets a power of 2. The exit code tells you exactly which tests failed.

---

## 27.2 Swap Values

Assignment returns the previous value, so `a = b = a` is a swap:

```verona
var a = 1;
var b = 2;
a = b = a;                           // a = 2, b = 1
```

See [Gotchas §26.3](26-gotchas.md) for the full explanation.

---

## 27.3 Implementing Comparison Operators

Define `<` and `==`, derive the rest:

```verona
mytype
{
  val: i32;

  ==(self: mytype, other: mytype): bool
  {
    self.val == other.val
  }

  <(self: mytype, other: mytype): bool
  {
    self.val < other.val
  }

  !=(self: mytype, other: mytype): bool { !(self == other) }
  <=(self: mytype, other: mytype): bool { !(other < self) }
  >(self: mytype, other: mytype): bool { other < self }
  >=(self: mytype, other: mytype): bool { !(self < other) }
}
```

---

## 27.4 Builder Pattern with Method Chaining

Methods that return `self` enable builder-style chaining. However, since Verona objects are mutable by default, you can use mutation directly:

```verona
config
{
  width: i32;
  height: i32;
  title: string;
}

main(): i32
{
  let c = config(800, 600, "default");
  c.width = 1920;
  c.height = 1080;
  c.title = "My App";
  c.width
}
```

---

## 27.5 Iterating with Index

The `for` loop doesn't provide an index directly. Track it with a `var`:

```verona
var index = 0;
for arr.values() elem ->
{
  // use index and elem
  process(index, elem);
  index = index + 1
}
```

---

## 27.6 Early Exit from Search

Use `raise` inside a block lambda for early exit from a loop or repeated operation:

```verona
find(arr: array[i32], target: i32): i32
{
  let check = (x: i32) -> {
    if x == target { raise x }
  }

  for arr.values() elem ->
  {
    check(elem)
  }

  0                                    // not found — default return
}
```

When `raise x` executes, control returns immediately from `find` with value `x`.

---

## 27.7 Optional Values with `nomatch`

Use `T | nomatch` for functions that may not find a result:

```verona
find_positive(arr: array[i32]): i32 | nomatch
{
  for arr.values() elem ->
  {
    if elem > 0 { return elem }
  }
  nomatch
}
```

The caller handles the `nomatch` case:

```verona
let result = find_positive(data) else { 0 }
// result is i32 — nomatch stripped by else
```

---

## 27.8 Type Dispatch with Match

Use `match` for type-based dispatch on union types:

```verona
process(val: i32 | string): i32
{
  match val
  {
    (n: i32) -> n * 2;
    (s: string) -> s.size.i32;
  }
  else (0)
}
```

---

## 27.9 Generic Containers

Create reusable containers using type parameters and shapes:

```verona
pair[A, B]
{
  first: A;
  second: B;
}

main(): i32
{
  let p = pair[i32, string](42, "hello");
  p.first + p.second.size.i32
}
```

---

## 27.10 Calling `apply` on Field Values

When a field holds a callable value (a lambda or object with `apply`), use parentheses or `.apply` to call it:

```verona
holder
{
  action: i32 -> i32;
}

main(): i32
{
  let h = holder((x: i32): i32 -> { x + 1 });
  let result = (h.action)(5);         // calls apply on the field value
  result
}
```

Remember: `h.action(5)` would try to call a method named `action` with arg `5`, which is different from calling `apply` on the field. See [Gotchas §26.5](26-gotchas.md).

---

## 27.11 Default-Filled Arrays

Create arrays with a default value and override specific elements:

```verona
let arr = array[i32]::fill(5, 99);
arr(0) = 1;
arr(2) = 3;
// arr is now [1, 99, 3, 99, 99]
```

Or create with defaults and fill in a loop:

```verona
let arr = array[i32]::fill(10);
var i = 0;
while i < arr.size
{
  arr(i) = (i * i).i32;
  i = i + 1
}
```

---

## 27.12 Partial Application for Callbacks

Fix some arguments to create a callback:

```verona
add(a: i32, b: i32): i32 { a + b }

let increment = add(1, _);           // i32 -> i32
let result = increment(41);          // 42
```

See [Partial Application](14-partial-application.md) for more patterns.

---

## 27.13 FFI Callbacks

Pass a Verona lambda as a C function pointer to an external library:

```verona
use "eventlib"
{
  set_handler = "set_handler"(ptr): none;
  clear_handler = "clear_handler"(): none;
}

main(): i32
{
  let handler = callback((): none -> { /* handle event */ });
  :::set_handler(handler.apply);
  // ... use the library ...
  :::clear_handler();
  handler.free;
  0
}
```

Key points:
- `callback(lambda)` creates a C-compatible closure (constructor sugar for `callback::create`).
- `.apply` returns the `ptr` to pass to C.
- `.free` must be called when the callback is no longer needed to release `libffi` resources.
- Callbacks registered via `ffi::register_external_notify` are automatically freed at program exit.

See [FFI §17.7](17-ffi.md) for full callback documentation.

---

## 27.14 External Resource Lifecycle

Use external resources to keep the scheduler alive while waiting for external events:

```verona
main(): i32
{
  let work = when ()
  {
    ffi::external.add;                // keep scheduler alive

    // ... do async work, wait for external event ...

    ffi::external.remove;             // allow scheduler to shut down
    0
  }

  0
}
```

The scheduler will not exit while external resources remain. See [FFI §17.8](17-ffi.md).

---

## 27.15 Global Singleton with `once`

Use `once` to create a global singleton that is initialized before `main()`:

```verona
global_state
{
  counter: i32;

  once create(): global_state
  {
    new {counter = 0}
  }
}

main(): i32
{
  let gs = global_state;              // always returns the same instance
  let gs2 = global_state;             // same instance as gs
  0
}
```

This pattern is commonly used in `_builtin/ffi/` for managing external resource state (e.g., the `external` class in `notify.v`). See [Functions §7.9](07-functions.md).
