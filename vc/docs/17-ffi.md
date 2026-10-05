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

### Initialization Requirements

Every reachable FFI operation whose library defines an `init` function adds a
compiler dependency on that initializer:

```verona
use "libmath"
{
  init(): any
  {
    :::configure();
  }

  configure = "configure"(): none;
  calculate = "calculate"(f64): f64;
}
```

A reachable `:::calculate(...)` operation adds a dependency on the `"libmath"`
so startup runs the initializer before `main`. The initializer's own call to
`:::configure()` produces a reflexive dependency on the same initializer,
which the compiler ignores. This also works when the initializer reaches the
operation through ordinary Verona helpers.

A call from one library's initializer to another library creates an ordinary
dependency, so the other library initializes first. A non-reflexive cycle
between initializers is a compile error. Emitted FFI operations remain direct
native calls. If the library has no `init` function, no initialization
dependency is added.

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

Most built-in operations can only appear in the `_builtin` package.
`add_external` and `remove_external` are global built-ins available to user
code.

### Built-in Categories

This table summarizes the common user-visible categories; it is not exhaustive.

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
| **Callback** | `make_callback`, `free_callback` |
| **External** | `add_external`, `remove_external` |

---

## 17.4 FFI vs Built-in Dispatch

When the compiler encounters `:::name(args)`:

1. If `name` matches a global built-in (`add_external` or `remove_external`),
   it becomes a built-in operation node.
2. If `name` matches another known built-in and the call is inside the
   `_builtin` package, it becomes a built-in operation node.
3. If `name` matches another known built-in outside `_builtin`, the compiler
   produces an error: "Builtin operators can only appear in `_builtin`".
4. Otherwise, it becomes an FFI call to the declared external symbol.

FFI calls go through `libffi` at runtime.

### Name Collisions

You cannot call an FFI declaration through the same Verona name as a built-in
operation such as `add`, `sub`, or `eq`. Dispatch checks built-in names first,
so an FFI declaration with that Verona name is shadowed. If the native C symbol
has a built-in name, expose it under a different Verona name:

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
- callback objects are passed as pointers to their fields; use the specific
  callback instance's `raw` method, such as `handler.raw`
  to pass the C function pointer

To explicitly create a raw pointer value in Verona source, use the `_builtin/ffi`
wrapper:

```verona
let null_ptr = ffi::ptr::create(none);
let stack_ptr = ffi::ptr::create(x);
```

`ffi::ptr::create(x)` takes the address of `x`'s current runtime storage. The
resulting pointer is read-only and cannot be stored into Verona heap objects; it
is intended for immediate FFI-style use.

Strings are Verona objects containing a `data: array[u8]` field, so to pass string bytes to C, pass `my_string.data` (and usually `my_string.size`) rather than the string object itself.

### Memory Ownership

Memory allocated by Verona (objects, arrays, strings) is managed by Verona's region system. Memory allocated by C code is **not** tracked by Verona. If a C function returns a pointer to allocated memory, Verona will store it as a `ptr` but will not free it — the C side is responsible for cleanup.

### Thread Safety

Ordinary FFI calls made by scheduled Verona work run on scheduler threads.
Calls made by an FFI initializer run synchronously during startup. Native code
that invokes a Verona callback runs that callback synchronously on the calling
thread. Independent `when` blocks may call the same FFI function concurrently,
potentially on different threads. Verona does not add synchronization around
FFI calls — the external library must be thread-safe, or accesses must be
serialized through a shared cown.

---

## 17.6 Init Functions

FFI `use` blocks can contain an `init` function with an inline body. Unlike FFI symbol declarations, `init` is a real Verona function defined directly inside the `use` block:

```verona
use
{
  init(): any
  {
    library_setup::run();
    // return a lambda to run at shutdown
    { :::cleanup_lib(); }
  }

  setup_lib = "setup_lib"(): none;
  cleanup_lib = "cleanup_lib"(): none;
}

library_setup
{
  run(): none
  {
    :::setup_lib();
  }
}
```

### Behavior

- **`init`** is lowered to a hidden `once` function.
- `init` must be a non-`ref` function with no parameters. Startup invokes its
  hidden memo initializer without arguments.
- A reachable FFI operation adds a compiler dependency on that hidden
  initializer. The operation remains a direct FFI call, and startup ordering
  ensures the initializer has already run.
- Initializers and source `once` functions share the same dependency ordering.
  Calls from an initializer to its own library are reflexive dependencies and
  are allowed. Non-reflexive cyclic initialization dependencies are compile
  errors.
- The `init` function has an inline body — it is **not** an FFI symbol binding like other `use` block entries.
- Examples use the return type `any` because an initializer may produce either
  `none` or a callable. A callable result must have exactly one eligible
  non-generic, non-`ref` `apply` method, accepting only `self`. The compiler
  registers that method as `@callback`; the emitted `AtTeardown` operation
  looks it up and schedules it as a **teardown callback**. Teardown callbacks
  run as last-in, first-out scheduler work across quiescent phases after
  ordinary work drains.
- If `init` returns `none`, no teardown callback runs. Any other returned value
  must satisfy the callback contract above. Invalid values and wrong-arity
  `apply` methods are not currently rejected statically and fail at runtime.
- If a teardown callback uses another initialized library or source `once`
  value, the compiler orders that dependency before this initializer so
  last-in, first-out teardown keeps the dependency live. See
  [Initialization and Teardown §28.5](28-initialization-and-teardown.md).
- `init` is optional. Across `use` blocks in the same enclosing class, a given
  library may have at most one `init`; declarations in different classes are
  independent.
- **Reification requirement:** Init functions are only included in the compiled
  output when a reachable FFI call requires them.

### Example: Library Lifecycle

```verona
use
{
  init(): any
  {
    var x: i32 = 1;
    :::init_printval(x);              // self-call during initialization
    let y: i32 = 3;
    { :::printval(y); }               // returned lambda runs after main
  }

  init_printval = "printval"(any): none;
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

### Init Returning a Teardown Callback

The last expression in the `init` body is returned. If it is a lambda or any
other valid callable, the compiler registers its method and `AtTeardown`
schedules it as the teardown callback for that library. This is the only way
to get shutdown behavior; there is no separate `fini` keyword.

---

## 17.7 Callbacks

Verona supports creating C-compatible function pointer callbacks from Verona lambdas. This allows Verona code to pass callable function pointers to external C libraries.

### The `ffi::callback` Type

The `ffi::callback` class (defined in `_builtin/ffi/callback.v`) wraps a Verona
callable in a C-compatible closure:

```verona
// Create a callback from a lambda
let cb = ffi::callback(my_lambda);

// Get the C function pointer (as ptr)
let fptr = cb.raw;
```

### API

| Operation | Description |
|-----------|-------------|
| `ffi::callback(callable)` | Create a callback wrapping `callable` (constructor sugar for `ffi::callback[T]::create`) |
| `cb.raw` | Get the C function pointer as `ptr` |

### Under the Hood

`ffi::callback[T]::create` calls `:::make_callback(callable)`, which uses
`libffi` to create a closure. The closure captures the Verona callable and
presents a C-compatible function pointer. Calling that function pointer from C
invokes the Verona lambda synchronously on the calling thread.

### Example: Passing a Callback to C

```verona
use "eventlib"
{
  // This example assumes register_handler invokes the callback before
  // returning and does not retain the pointer.
  register_handler = "register_handler"(ptr): none;
}

main(): i32
{
  let handler = ffi::callback((): none -> { /* handle event */ });
  :::register_handler(handler.raw);
  0
}
```

Passing `handler.raw` does not retain `handler`. If native code stores the
function pointer for later use, the Verona program must keep the callback object
alive until the handler is deregistered. Pin the callback or retain it in a
process-lifetime value, then release it only after the native library can no
longer call the pointer and all in-progress calls have returned. Native code may
invoke the same callback concurrently on multiple calling threads; Verona does
not serialize those invocations, so captured state must be safe for that access
pattern. Asynchronous native resources should also use the external-resource
tracking wrappers described below.

---

## 17.8 The `_builtin/ffi` Module

The `_builtin/ffi/` directory contains Verona wrapper functions for common FFI operations. These provide a higher-level interface and are accessed via the `ffi::` namespace:

### Available Wrappers

| Function | Signature | Description |
|----------|-----------|-------------|
| `ffi::external.add` | `(self: external): none` | Add an external resource (increments the external event count) |
| `ffi::external.remove` | `(self: external): none` | Remove an external resource (decrements the external event count) |
| `ffi::pin(x)` | `(x: any): none` | Pin a refcounted Verona value (object, array, or cown) for external use. Pinning a readonly object or array is a runtime error. |
| `ffi::unpin(x)` | `(x: any): none` | Release a prior external pin |
| `ffi::struct[A]` | layout helper object | Compute and cache C ABI layout metadata for a single FFI-compatible field type `A` or a flat tuple `(A1, A2, ...)` |

### External Resource Management

The runtime tracks "external resources" — things outside the Verona scheduler's control (open file descriptors, active network connections, pending OS callbacks). The scheduler waits for all external resources to be removed before shutting down:

- **`ffi::external.add`** — Tells the scheduler an external resource exists. The scheduler will not shut down while external resources remain.
- **`ffi::external.remove`** — Tells the scheduler an external resource has been released.
- **`ffi::pin(x)` / `ffi::unpin(x)`** — Add or remove an external root for a refcounted Verona value. This is intended for low-level FFI wrappers that hand Verona-managed memory to external code. Objects, arrays, and cowns can be pinned. Pinning a frame-local object or array first drags it to a fresh heap region. Pinning a stack allocation is an error.

### Raw Struct Layout Helpers

`ffi::struct[A]()` memoizes C ABI layout metadata for `A`. If `A` is a bare type, it is treated as a single-field struct. If `A` is a tuple, it must be a **flat** tuple of field types. Nested tuples are rejected.

```verona
let layout = ffi::struct[(u8, i32, usize)]();
let mem = layout.alloc;

layout.store[u8](mem, 0, u8 7);
layout.store[i32](mem, 1, i32 5);
layout.store[usize](mem, 2, 1234);

let x = layout.load[i32](mem, 1);
layout.free(mem);
```

The layout object contains:

- `size: usize` — total struct size
- `offsets: array[usize]` — byte offset of each field
- `kinds: array[u8]` — runtime kind tags used to validate `load[B]` / `store[B]`

Allowed field types are the FFI-compatible leaf types: primitive scalars, `ptr`, objects, arrays, and cowns. `dyn`, `ref[T]`, unions, and tuples-as-fields are rejected.

`load[B]` / `store[B]` check that `B` matches the recorded field kind, then read or write raw memory at `ptr + offsets(index)`. This is intentionally low-level: the runtime can validate the expected Verona kind, but it cannot prove that an arbitrary foreign memory block really contains a valid Verona-managed object graph.

If you store Verona-managed pointer-like values (objects, arrays, cowns) into foreign memory, you are responsible for keeping them alive for as long as the foreign code may use them. Use `ffi::pin` / `ffi::unpin` when appropriate.

The `external` class is a singleton (using `once create()`) that serializes add/remove operations through an internal cown. The dot syntax `ffi::external.add` auto-calls `create()` to get the singleton and then dispatches `.add` on it. See [Functions §7.10](07-functions.md) for more on `once` functions.

### How `_builtin/ffi` Works

Each `.v` file in `_builtin/ffi/` defines either a class (like `callback`, `external`) or free functions. Because `_builtin` is always implicitly imported, and `_builtin/ffi/` is a nested scope, these are accessible via `ffi::function_name(args)` or `ffi::class_name.method`.

The wrappers internally use `:::` builtins:
- `ffi::callback[T]::create` uses `:::make_callback`
- `external` uses `once create()` for singleton initialization and serializes `:::add_external`/`:::remove_external` through an internal cown
- `add_external` and `remove_external` call their corresponding `:::` builtins
- `struct[A]` uses `:::ffistruct[A]`, `:::ffiload[B]`, and `:::ffistore[B]`
