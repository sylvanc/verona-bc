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

1. If `name` matches a known built-in (from the table above), it becomes a built-in operation node.
2. Otherwise, it becomes an FFI call to the declared external symbol.

FFI calls go through `libffi` at runtime.
