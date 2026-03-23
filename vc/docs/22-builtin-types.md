# 22. Built-in Types Reference

[← Table of Contents](README.md) | [Previous: Toolchain Usage](21-toolchain-usage.md) | [Next: Grammar Summary →](23-grammar-summary.md)

Complete reference for all types defined in the `_builtin` module.

---

## 22.1 Integer Types

Twelve integer types, all with the same method set:

| Signed | Unsigned | Size |
|--------|----------|------|
| `i8` | `u8` | 8-bit |
| `i16` | `u16` | 16-bit |
| `i32` | `u32` | 32-bit |
| `i64` | `u64` | 64-bit |
| `ilong` | `ulong` | Platform-width (C `long`) |
| `isize` | `usize` | Pointer-width |

### Constructors

```verona
i32(42)                               // calls i32::create(42)
i32                                   // calls i32::create() — default value (0)
```

`create(some: T = 0): T` — constructor with default.

### Arithmetic

| Method | Signature | Description |
|--------|-----------|-------------|
| `+` | `+(self, other): T` | Addition |
| `-` | `-(self, other): T` | Subtraction |
| `*` | `*(self, other): T` | Multiplication |
| `/` | `/(self, other): T` | Division |
| `%` | `%(self, other): T` | Modulo |
| `-` | `-(self): T` | Unary negation (signed types only) |

### Bitwise

| Method | Signature | Description |
|--------|-----------|-------------|
| `&` | `&(self, other): T` | Bitwise AND |
| `\|` | `\|(self, other): T` | Bitwise OR |
| `^` | `^(self, other): T` | Bitwise XOR |
| `<<` | `<<(self, other): T` | Left shift |
| `>>` | `>>(self, other): T` | Right shift |
| `!` | `!(self): T` | Bitwise NOT |

### Comparison

| Method | Signature | Returns |
|--------|-----------|---------|
| `==` | `==(self, other): bool` | Equal |
| `!=` | `!=(self, other): bool` | Not equal |
| `<` | `<(self, other): bool` | Less than |
| `<=` | `<=(self, other): bool` | Less than or equal |
| `>` | `>(self, other): bool` | Greater than |
| `>=` | `>=(self, other): bool` | Greater than or equal |

### Other Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `min` | `min(self, other): T` | Minimum of two values |
| `max` | `max(self, other): T` | Maximum of two values |

### Overflow Behavior

Integer arithmetic follows the behavior of the underlying platform:

- **Unsigned types** (`u8`, `u16`, `u32`, `u64`, `usize`, `ulong`): overflow **wraps** using modular arithmetic. This is well-defined — `u8 255 + (u8 1)` produces `0`.
- **Signed types** (`i8`, `i16`, `i32`, `i64`, `isize`, `ilong`): overflow is **undefined behavior** (as in C/C++). In practice on modern hardware, signed overflow typically wraps on two's complement architectures, but the compiler may optimize under the assumption that it does not occur.
- **Floating point** (`f32`, `f64`): follows IEEE 754 rules — overflow produces `inf` or `-inf`, invalid operations produce `nan`.

There are currently no checked-arithmetic or saturating-arithmetic variants. If you need overflow detection, check before the operation.

### Division by Zero

Integer division by zero is a runtime error. Floating-point division by zero produces `inf`, `-inf`, or `nan` per IEEE 754.

### Type Conversions

Every integer type has conversion methods to every other numeric type:

```verona
let x: i32 = 42;
x.i64                                 // convert to i64
x.u32                                 // convert to u32
x.f64                                 // convert to f64
x.usize                              // convert to usize
```

Full list: `.i8`, `.i16`, `.i32`, `.i64`, `.u8`, `.u16`, `.u32`, `.u64`,
`.ilong`, `.ulong`, `.isize`, `.usize`, `.f32`, `.f64`.

---

## 22.2 Floating Point Types

### `f32`

32-bit IEEE 754 float. Has the same methods as `f64`:

**Arithmetic:** `+`, `-`, `*`, `/`, `%`, `**`, unary `-`
**Comparison:** `==`, `!=`, `<`, `<=`, `>`, `>=`
**Math:** `min`, `max`, `logbase`, `atan2`, `abs`, `ceil`, `floor`, `exp`, `log`, `sqrt`, `cbrt`
**Trig:** `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`
**Predicates:** `isinf`, `isnan`
**Constants:** `e`, `pi`, `inf`, `nan`
**Conversions:** `.bool`, all numeric conversions (`.i8` through `.f64`)

### `f64`

64-bit IEEE 754 float. Same method set as `f32`.

### Arithmetic

Same as integers: `+`, `-`, `*`, `/`, `%`, unary `-`.

Plus:
| Method | Signature | Description |
|--------|-----------|-------------|
| `**` | `**(self, other): f64` | Exponentiation |

### Comparison

Same as integers: `==`, `!=`, `<`, `<=`, `>`, `>=`.

Plus:
| Method | Signature | Description |
|--------|-----------|-------------|
| `min` | `min(self, other): f64` | Minimum |
| `max` | `max(self, other): f64` | Maximum |

### Mathematical Functions

| Method | Description |
|--------|-------------|
| `abs` | Absolute value |
| `ceil` | Ceiling (round up) |
| `floor` | Floor (round down) |
| `exp` | e^x |
| `log` | Natural logarithm |
| `logbase(other)` | Logarithm base `other` |
| `sqrt` | Square root |
| `cbrt` | Cube root |

### Trigonometric Functions

| Method | Description |
|--------|-------------|
| `sin`, `cos`, `tan` | Standard trig |
| `asin`, `acos`, `atan` | Inverse trig |
| `atan2(other)` | Two-argument arctangent |
| `sinh`, `cosh`, `tanh` | Hyperbolic |
| `asinh`, `acosh`, `atanh` | Inverse hyperbolic |

### Queries

| Method | Returns | Description |
|--------|---------|-------------|
| `isinf` | `bool` | Is infinity? |
| `isnan` | `bool` | Is NaN? |

### Constants (Static Methods)

```verona
f64::e                                // Euler's number
f64::pi                               // Pi
f64::inf                              // Positive infinity
f64::nan                              // Not-a-Number
```

### Conversions

`.bool` (non-zero → true), plus all numeric conversions (same as integers).

---

## 22.3 `bool`

### Constructor

```verona
bool                                  // default: false
bool(true)                            // explicit
```

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `==` | `==(self, other: bool): bool` | Equality |
| `!=` | `!=(self, other: bool): bool` | Inequality |
| `<`, `<=`, `>`, `>=` | Ordering | Comparison operators |
| `&` | `&(self, other: to_bool): bool` | Short-circuit AND |
| `\|` | `\|(self, other: to_bool): bool` | Short-circuit OR |
| `^` | `^(self, other: bool): bool` | XOR |
| `!` | `!(self): bool` | Logical NOT |
| `min` | `min(self, other: bool): bool` | Minimum |
| `max` | `max(self, other: bool): bool` | Maximum |
| `apply` | `apply(self): bool` | Identity |
| `bool` | `bool(self): bool` | Identity |

### Short-Circuit Semantics

`&` and `|` take a `to_bool` shape (not `bool`) as the right operand:

```verona
shape to_bool { apply(self: self): bool; }
```

The RHS is evaluated only if needed — `&` skips if `self` is `false`, `|` skips if `self` is `true`.

---

## 22.4 `string`

### Internal Representation

`string` contains `data: array[u8]`.

### Constructor

```verona
string(data: array[u8])              // from u8 array
```

String literals (`"hello"`) produce `string` values.

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `size` | `size(self): usize` | Length in bytes |
| `ref apply` | `ref apply(self, index: usize): ref[u8]` | Ref to byte at index |
| `bool` | `bool(self): bool` | Non-empty? |
| `+` | `+(self, other: string): string` | Concatenation |
| `==`, `!=` | Equality/inequality | Byte-by-byte comparison |
| `<`, `<=`, `>`, `>=` | Ordering | Lexicographic comparison |

---

## 22.5 `array[T]`

### Creation

```verona
array[i32]::fill(10)                 // 10 default-filled elements
array[i32]::fill(3, 42)              // 3 elements, each 42
::(i32 1, 2, 3)                      // array literal (sibling refinement)
```

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `fill` | `fill(size: usize, from: T = T): array[T]` | Create array (static) |
| `apply` | `apply(self, index: usize): T` | Read element |
| `ref apply` | `ref apply(self, index: usize): ref[T]` | Write element |
| `size` | `size(self): usize` | Number of elements |
| `each` | `each(self, f: T -> none): none` | Call `f` on each element |
| `pairs` | `pairs(self, f: (usize, T) -> none): none` | Call `f` with index and element |

---

## 22.6 `ref[T]`

Mutable reference wrapper. Used internally for field and array element writes.

| Method | Signature | Description |
|--------|-----------|-------------|
| `ref *` | `ref *(self): ref[T]` | Dereference (returns self — identity for LHS access) |

---

## 22.7 `cown[T]`

Concurrent ownership wrapper. See [Concurrency](15-concurrency.md).

| Method | Signature | Description |
|--------|-----------|-------------|
| `read` | `read(self): cown[T]` | Read-only access |

---

## 22.8 `none`

Unit type — represents the absence of a meaningful value.

```verona
none                                  // calls none::create()
```

---

## 22.9 `nomatch`

Sentinel type for failed pattern matches and iterator exhaustion:

```verona
nomatch                               // calls nomatch::create()
```

Used in the return type of iterator `.next()` methods (`T | nomatch`) and as the sentinel returned by failed `match` arms. When a `match` type test or value test doesn't match, the arm returns `nomatch`, which chains into the next arm or the `else` fallback. Without `else`, a non-exhaustive match returns `nomatch`. See [Control Flow §6.8](06-control-flow.md).

---

## 22.10 `any`

The universal shape — satisfied by every type:

```verona
shape any {}
```

Used as a parameter type when no specific methods are needed (e.g., `is()`, `bits()`, FFI declarations).

---

## 22.11 `ptr`

Opaque raw pointer type. Empty class with no methods:

```verona
ptr {}
```

Used to pass C function pointers and opaque handles through FFI. See [FFI §17.7](17-ffi.md) for callback usage.

---

## 22.12 `callback`

C-compatible function pointer wrapper. Defined in `_builtin/ffi/callback.v`:

```verona
callback
{
  create[T](callable: T): callback    // wrap a Verona callable
  apply(self: callback): ptr          // get the C function pointer
  free(self: callback): none          // free the closure
}
```

| Method | Signature | Description |
|--------|-----------|-------------|
| `create[T]` | `create(callable: T): callback` | Wrap a callable (lambda, object with `apply`) in a C closure |
| `apply` | `apply(self): ptr` | Get the C-compatible function pointer |
| `free` | `free(self): none` | Free the underlying `libffi` closure |

Constructor sugar: `callback(my_lambda)` calls `callback::create(my_lambda)`.

See [FFI §17.7](17-ffi.md) for full callback documentation.

---

## 22.13 FFI Module Functions

Free functions in `_builtin/ffi/`:

| Function | Signature | Description |
|----------|-----------|-------------|
| `ffi::external.add` | `(self: external): none` | Increment the external resource count |
| `ffi::external.remove` | `(self: external): none` | Decrement the external resource count |

The `external` singleton is accessed as `ffi::external` (calls `external::create()`), and its methods are called via dot syntax: `ffi::external.add`, `ffi::external.remove`.

See [FFI §17.8](17-ffi.md) for external resource management details.

---

## 22.14 Identity Functions

Free functions in `_builtin/is.v`:

| Function | Signature | Description |
|----------|-----------|-------------|
| `is` | `is(a: any, b: any): bool` | Pointer identity equality |
| `isnt` | `isnt(a: any, b: any): bool` | Pointer identity inequality |
| `bits` | `bits(a: any): u64` | Raw pointer address |
