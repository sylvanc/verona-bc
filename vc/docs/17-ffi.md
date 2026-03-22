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
| **Callback** | `make_callback`, `callback_ptr`, `free_callback` |
| **External** | `add_external`, `remove_external` |

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

For FFI parameters declared as `ptr`, Verona also passes pointer-like runtime values by their underlying C representation:

- `none` becomes `NULL`
- arrays are passed as a pointer to their element storage
- objects are passed as a pointer to their fields (like a C `struct`)
- `callback` values are passed as their C function pointer

Strings are Verona objects containing a `data: array[u8]` field, so to pass string bytes to C, pass `my_string.data` (and usually `my_string.size`) rather than the string object itself.

### Memory Ownership

Memory allocated by Verona (objects, arrays, strings) is managed by Verona's region system. Memory allocated by C code is **not** tracked by Verona. If a C function returns a pointer to allocated memory, Verona will store it as a `ptr` but will not free it — the C side is responsible for cleanup.

### Thread Safety

FFI calls run on scheduler threads. If two `when` blocks both call the same FFI function, they run on different threads. Verona does not add synchronization around FFI calls — the external library must be thread-safe, or accesses must be serialized through a shared cown.

---

## 17.6 Init Functions

FFI `use` blocks can contain an `init` function with an inline body. Unlike FFI symbol declarations, `init` is a real Verona function defined directly inside the `use` block:

```verona
use
{
  init(): any
  {
    // initialization code — runs before main()
    setup_lib();
    // return a lambda to run at shutdown
    { cleanup_lib(); }
  }

  setup_lib = "setup_lib"(): none;
  cleanup_lib = "cleanup_lib"(): none;
}
```

### Behavior

- **`init`** runs once, before `main()`, on a scheduler thread.
- The `init` function has an inline body — it is **not** an FFI symbol binding like other `use` block entries.
- `init` returns `any`. If the return value is a callable (a lambda or an object with `apply`), the runtime calls it as a **finalizer** after `main()` and all pending `when` behaviors complete, just before process exit.
- If `init` returns `none` or a non-callable value, no finalizer runs.
- `init` is optional — a `use` block can have zero or one `init` function. Multiple `init` functions for the same library are a compile error.
- **Reification requirement:** Init functions are only included in the compiled output when at least one FFI symbol from the same `use` block is called from reachable code. If no FFI call reaches the library, the init function will not run. This means a test exercising init behavior must include at least one reachable FFI call from the same `use` block.

### Example: Library Lifecycle

```verona
use
{
  init(): any
  {
    var x: i32 = 1;
    :::printval(x);                   // runs before main
    let y: i32 = 3;
    { :::printval(y); }               // returned lambda runs after main
  }

  printval = "printval"(any): none;
}

main(): i32
{
  var x: i32 = 2;
  :::printval(x);                     // runs during main
  0
}
// Output: 1, 2, 3
```

### Init Returning a Finalizer

The last expression in the `init` body is returned. If it's a lambda (or any callable), the runtime registers it as the finalizer for that library. This is the only way to get shutdown behavior — there is no separate `fini` keyword.

---

## 17.7 Callbacks

Verona supports creating C-compatible function pointer callbacks from Verona lambdas. This allows Verona code to pass callable function pointers to external C libraries.

### The `callback` Type

The `callback` class (defined in `_builtin/ffi/callback.v`) wraps a Verona callable in a C-compatible closure:

```verona
// Create a callback from a lambda
let cb = callback(my_lambda);

// Get the C function pointer (as ptr)
let fptr = cb.apply;

// Free the callback when done (releases the closure)
cb.free;
```

### API

| Operation | Description |
|-----------|-------------|
| `callback(callable)` | Create a callback wrapping `callable` (constructor sugar for `callback::create`) |
| `cb.apply` | Get the C function pointer as `ptr` |
| `cb.free` | Free the underlying closure resources |

### Under the Hood

`callback::create[T]` calls `:::make_callback(callable)`, which uses `libffi` to create a closure. The closure captures the Verona callable and presents a C-compatible function pointer that, when called from C, invokes the Verona lambda on the scheduler thread.

### Example: Passing a Callback to C

```verona
use "eventlib"
{
  register_handler = "register_handler"(ptr): none;
}

main(): i32
{
  let handler = callback((): none -> { /* handle event */ });
  :::register_handler(handler);
  // ... later:
  handler.free;
  0
}
```

---

## 17.8 The `_builtin/ffi` Module

The `_builtin/ffi/` directory contains Verona wrapper functions for common FFI operations. These provide a higher-level interface and are accessed via the `ffi::` namespace:

### Available Wrappers

| Function | Signature | Description |
|----------|-----------|-------------|
| `ffi::external.add` | `(self: external): none` | Add an external resource (increments the external event count) |
| `ffi::external.remove` | `(self: external): none` | Remove an external resource (decrements the external event count) |

### External Resource Management

The runtime tracks "external resources" — things outside the Verona scheduler's control (open file descriptors, active network connections, pending OS callbacks). The scheduler waits for all external resources to be removed before shutting down:

- **`ffi::external.add`** — Tells the scheduler an external resource exists. The scheduler will not shut down while external resources remain.
- **`ffi::external.remove`** — Tells the scheduler an external resource has been released.

The `external` class is a singleton (using `once create()`) that serializes add/remove operations through an internal cown. The dot syntax `ffi::external.add` auto-calls `create()` to get the singleton and then dispatches `.add` on it. See [Functions §7.10](07-functions.md) for more on `once` functions.

### How `_builtin/ffi` Works

Each `.v` file in `_builtin/ffi/` defines either a class (like `callback`, `external`) or free functions. Because `_builtin` is always implicitly imported, and `_builtin/ffi/` is a nested scope, these are accessible via `ffi::function_name(args)` or `ffi::class_name.method`.

The wrappers internally use `:::` builtins:
- `callback::create[T]` uses `:::make_callback`
- `external` uses `once create()` for singleton initialization and serializes `:::add_external`/`:::remove_external` through an internal cown
- `add_external` and `remove_external` call their corresponding `:::` builtins
