# 17. FFI (Foreign Function Interface)

[← Table of Contents](README.md) | [Previous: Modules](16-modules.md) | [Next: Type Inference →](18-type-inference.md)

This chapter covers interfacing with external code through the FFI and built-in operations.

---

## 17.1 FFI Declarations

External functions are declared with a `use` block:

```verona
use
{
  printval = "printval"(any): none;
}
```

This declares `printval` as an external symbol that takes `any` and returns `none`. The left side is the Verona name; the quoted string is the symbol name in the shared library.

### With a Library Name

```verona
use "libmath"
{
  fast_sin = "sin"(f64): f64;
  fast_cos = "cos"(f64): f64;
}
```

### Variadic Functions

Use `...` at the end of the parameter list:

```verona
use "libc"
{
  printf = "printf"(ptr, ...): i32;
}
```

---

## 17.2 Calling FFI Functions

Once declared, FFI functions are called with the `:::` prefix:

```verona
:::printval(my_value);
:::fast_sin(3.14);
```

---

## 17.3 Built-in Operations (`:::`)

The `:::` prefix is also used for built-in operations defined by the runtime. These are the primitive operations that the `_builtin` types delegate to:

```verona
// From _builtin/i32.v:
+(self: i32, other: i32): i32 { :::add(self, other) }
-(self: i32, other: i32): i32 { :::sub(self, other) }
==(self: i32, other: i32): bool { :::eq(self, other) }
```

Built-in operations can only appear in the `_builtin` package.

### Built-in Categories

| Category | Operations |
|----------|------------|
| **Arithmetic** | `add`, `sub`, `mul`, `div`, `mod`, `neg` |
| **Power/Math** | `pow`, `abs`, `ceil`, `floor`, `exp`, `log`, `sqrt`, `cbrt` |
| **Trig** | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` |
| **Hyperbolic** | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh` |
| **Log** | `logbase` |
| **Bitwise** | `and`, `or`, `xor`, `shl`, `shr`, `not` |
| **Comparison** | `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `min`, `max` |
| **Conversion** | `convi8`, `convi16`, `convi32`, `convi64`, `convu8`, `convu16`, `convu32`, `convu64`, `convf32`, `convf64`, `convilong`, `convulong`, `convisize`, `convusize` |
| **Identity** | `eq` (pointer), `ne` (pointer), `bits` |
| **Constants** | `none`, `e`, `pi`, `inf`, `nan` |
| **Memory** | `len`, `ptr`, `read`, `arrayref`, `newarray` |

---

## 17.4 FFI vs Built-in Dispatch

When the compiler encounters `:::name(args)`:

1. If `name` matches a known built-in (from the table above) **and** the call is inside the `_builtin` package, it becomes a built-in operation node.
2. If `name` matches a known built-in **but** the call is outside `_builtin`, the compiler produces an error: "Builtin operators can only appear in `_builtin`".
3. Otherwise, it becomes an FFI call to the declared external symbol.

FFI calls go through `libffi` at runtime.

### Name Collisions

You **cannot** define an FFI function with the same name as a built-in operation (like `add`, `sub`, `eq`). The compiler checks built-in names first. If the name matches, it requires the call to be inside `_builtin` — preventing user code from shadowing builtins. If you need an FFI function whose C symbol name conflicts with a builtin, use a different Verona name:

```verona
use "mylib"
{
  my_add = "add"(i32, i32): i32;     // Verona name: my_add, C symbol: "add"
}
```

---

## 17.5 FFI Practical Notes

### Data Passing

Primitive types (`i32`, `f64`, `bool`, `ptr`, `usize`, etc.) are passed directly — they correspond to their C equivalents. The `ptr` type is an opaque raw pointer (see [Built-in Types §22.11](22-builtin-types.md)).

Strings and arrays are **not** directly passable as C data structures. Strings are Verona objects containing a `data: array[u8]` field; arrays are Verona objects with a header. To pass string data to C, use the `ptr` and `len` builtins from within `_builtin` to extract a raw pointer and length.

### Memory Ownership

Memory allocated by Verona (objects, arrays, strings) is managed by Verona's region system. Memory allocated by C code is **not** tracked by Verona. If a C function returns a pointer to allocated memory, Verona will store it as a `ptr` but will not free it — the C side is responsible for cleanup.

### Thread Safety

FFI calls run on scheduler threads. If two `when` blocks both call the same FFI function, they run on different threads. Verona does not add synchronization around FFI calls — the external library must be thread-safe, or accesses must be serialized through a shared cown.
