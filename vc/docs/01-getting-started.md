# 1. Getting Started

[← Table of Contents](README.md) | [Next: Program Structure →](02-program-structure.md)

Verona is a research programming language focused on concurrent ownership and memory safety. Programs are compiled by _`vc`_ to bytecode (`.vbc` files) and executed by the _`vbci`_ interpreter.

This chapter covers the minimal steps to build the toolchain, write a first program, compile it, and run it. It also describes the project layout conventions.

> **Current status:** Verona is a research language under active development. The language provides primitive types, fixed-size arrays, generics, shapes (structural interfaces), cowns (concurrent ownership), and a bytecode interpreter. There is **no standard library** beyond `_builtin` — collections, I/O, string formatting, and other facilities will be provided by packages as the ecosystem develops. The only output mechanism today is `:::printval` (an FFI debug hook) and process exit codes. See [Program Structure §2.6](02-program-structure.md) for the rationale.

---

## 1.1 Hello World

A Verona program needs exactly one thing: a `main` function that returns `i32`. The return value becomes the process exit code.

The smallest valid program:

```verona
main(): i32
{
  0
}
```

This program does nothing visible — it returns exit code `0` (success).

Here is a slightly more interesting example that performs arithmetic and returns the result as the exit code:

```verona
main(): i32
{
  let a: i32 = 3;
  let b: i32 = 7;
  a + b
}
```

This returns exit code `10`. Integer literals default to `u64`, so the `: i32` annotation tells the compiler to infer these as `i32`. The last expression in a block is its value — there is no `return` keyword needed (though `return` is available for early exits).

A program with a class, construction, field access, and mutation:

```verona
cell
{
  f: i32;
  g: i32;

  create(f: i32 = 0, g: i32 = 0): cell
  {
    new {f = f; g = g}
  }
}

main(): i32
{
  let a = cell;
  var b = cell(3, 7);
  a.f = 3;
  a.f + b.g
}
```

This returns exit code `10`. Notable syntax:
- Classes are declared with a bare name — no `class` keyword.
- Fields end with `;`. In `new { ... }`, fields are also separated by `;`.
- `new { f = f; g = g }` constructs an object with named field assignments.
- `cell` (no arguments) calls `create` with defaults — `cell::create` is called because `Type(args)` is sugar for `Type::create(args)`. The name `create` is not magical; it's simply the convention. Juxtaposition on a type name calls its `create` method.
- `cell(3, 7)` is sugar for `cell::create(3, 7)`.
- `let` bindings are single-assignment; `var` bindings are reassignable.
- `a.f = 3` writes to a field of a `let`-bound object — `let` constrains the binding, not the object.

> **Important: Flat operator precedence.** All infix operators (`+`, `*`, `<`, `==`, etc.) have **the same precedence**. `a + b * c` is parsed as `(a + b) * c`. Always use parentheses: `a + (b * c)`. See [Expressions §5.8](05-expressions.md) and [Gotchas §26.1](26-gotchas.md).

For more on each of these features, see the linked chapters in the [Table of Contents](README.md).

---

## 1.2 Building the Toolchain

### Prerequisites

- CMake 3.14+
- Ninja (recommended) or Make
- A C++23 compiler (GCC 13+ or Clang 17+)
- OpenSSL development headers (3.0+ recommended)
- libffi development headers (3.4+)

### Build Steps

```bash
git clone <repo-url> verona-bc
cd verona-bc
mkdir build && cd build
cmake .. -G Ninja
ninja install
```

`ninja install` compiles the project and installs the binaries to `build/dist/`. The two main executables are:

| Binary | Path | Purpose |
|--------|------|---------|
| `vc` | `build/dist/vc/vc` | Compiler — `.v` source → `.vbc` bytecode |
| `vbci` | `build/dist/vbci/vbci` | Interpreter — executes `.vbc` bytecode |

> **Important:** Always use the _installed_ binary at `dist/vc/vc`, not the build binary at `build/vc/vc`. The installed binary has the `_builtin` standard library directory next to it, which the compiler requires.

### Running the Tests

```bash
cd build
ctest --output-on-failure -j$(nproc)
```

---

## 1.3 Compiling and Running a Program

### Step 1: Create a Project Directory

A Verona project is a directory containing one or more `.v` source files. There is no manifest or configuration file — just source code.

```bash
mkdir hello
cat > hello/hello.v << 'EOF'
main(): i32
{
  0
}
EOF
```

### Step 2: Compile

```bash
build/dist/vc/vc build hello
```

This produces `hello.vbc` in the current directory. The output filename is derived from the project directory name.

To specify a different output file:

```bash
build/dist/vc/vc build hello -b my_program.vbc
```

### Step 3: Run

```bash
build/dist/vbci/vbci hello.vbc
echo $?    # prints the exit code (0)
```

The interpreter runs `main()` and uses its `i32` return value as the process exit code.

### Compiler Options

| Flag | Description |
|------|-------------|
| `-b <file>` | Set the output bytecode filename |
| `-s` | Strip debug information from the bytecode |
| `-p <pass>` | Stop compilation after a specific pass (for debugging) |
| `--dump_passes=<dir>` | Dump intermediate ASTs to a directory (for debugging) |

### Interpreter Options

| Flag | Description | Default |
|------|-------------|--------|
| `-t <N>` | Set the number of scheduler threads | Number of CPU cores |
| `-l <level>` | Set log level (`Trace`, `Debug`, `Info`, `Warning`, `Output`, `Error`, `None`) | `Error` |

---

## 1.4 Project Layout

### Source Project

A project that compiles with `vc build` is a directory of `.v` files:

```
my_project/
  main.v          # must contain main(): i32
  utils.v         # additional module (optional)
  types.v         # additional module (optional)
```

Each `.v` file defines a module named after the file (without the extension). Modules can reference each other's declarations using qualified names (`ModuleName::item`) or by importing with `use ModuleName`. Subdirectories are supported — they create nested module scopes (e.g., `ffi/callback.v` is accessible as `ffi::callback`). See [Modules and Imports](16-modules.md).

### The `_builtin` Standard Library

The compiler ships with a `_builtin` directory containing the standard library:

```
_builtin/
  any.v           shape any {} — universal interface
  array.v         array[T] — generic arrays and arrayiter[T]
  bool.v          bool — with short-circuit & and |
  cown.v          cown[T] — concurrent ownership
  f32.v           f32 — 32-bit float
  f64.v           f64 — 64-bit float
  ffi/            FFI wrappers — callback, external notify, ptr
    callback.v    callback — C-compatible function pointer wrapper
    notify.v      external class (singleton via once), register_external_notify
    ptr.v         ptr — opaque raw pointer type
  i8.v            i8 — 8-bit signed integer
  i16.v           i16 — 16-bit signed integer
  i32.v           i32 — 32-bit signed integer
  i64.v           i64 — 64-bit signed integer
  ilong.v         ilong — platform-width signed integer
  isize.v         isize — pointer-width signed integer
  is.v            is(), isnt(), bits() — identity and raw pointer operations
  nomatch.v       nomatch — sentinel for failed matches
  none.v          none — unit type
  ptr.v           ptr — raw pointer type
  ref.v           ref[T] — mutable reference wrapper
  string.v        string — backed by array[u8]
  u8.v            u8 — 8-bit unsigned integer
  u16.v           u16 — 16-bit unsigned integer
  u32.v           u32 — 32-bit unsigned integer
  u64.v           u64 — 64-bit unsigned integer
  ulong.v         ulong — platform-width unsigned integer
  usize.v         usize — pointer-width unsigned integer
```

The `_builtin` module is implicitly available — its types can be used without an explicit `use` declaration. For details on each type's methods, see [Built-in Types Reference](22-builtin-types.md).

### Bytecode Output

The compiler produces a single `.vbc` file. By default, its name is derived from the project directory:

```
vc build my_project/    →    my_project.vbc
```

Use `-b <filename>` to override the output name:

```bash
vc build my_project/ -b output.vbc
```

The `.vbc` file is a self-contained bytecode bundle that can be executed on any machine with the `vbci` interpreter.

---

## 1.5 A Larger Example

This program creates an array, fills it with values, sums them with a `for` loop, and returns the sum as the exit code:

```verona
main(): i32
{
  let arr = array[i32]::fill(10);
  var index = 0;

  while index < arr.size
  {
    arr(index) = index.i32;
    index = index + 1
  }

  var sum = 0;

  for arr.values() i ->
  {
    sum = sum + i
  }

  sum
}
```

This returns exit code `45` (the sum 0 + 1 + 2 + ... + 9).

Key features demonstrated:
- **Generic types**: `array[i32]::fill(10)` creates a 10-element array of `i32`, default-filled.
- **Type inference**: `var index = 0` — the literal `0` is inferred as `usize` from context (used with `arr.size` and as an array index).
- **Indexing**: `arr(index)` reads or writes via juxtaposition — this calls `arr.apply(index)` (or `arr.ref apply(index)` for writes). Verona uses juxtaposition instead of `[ ]` brackets for indexing. See [Expressions](05-expressions.md).
- **Type conversion**: `index.i32` converts `usize` to `i32`. See [Types](03-types.md).
- **Iterators and `->` blocks**: `for arr.values() i -> { ... }` calls `.next()` on the iterator, binding each element to `i`. The `->` introduces a lambda body — `i` is the parameter, and the `{ ... }` is the body. See [Arrays](12-arrays.md), [Lambdas](13-lambdas.md), and [Control Flow](06-control-flow.md).
- **Semicolons**: Required between sequential statements (`let arr = ...;` before `var index = 0;`), and after field definitions. Not required after the last expression in a block, or after closing `}` of control flow. See [Program Structure §2.5](02-program-structure.md).

---

## 1.5.1 A Complete Example: Shapes and Generics

This example demonstrates classes, shapes, generics, match expressions, and iteration working together. It defines a shape for values that can be "scored," creates two classes satisfying that shape, and uses a generic function to sum their scores:

```verona
// Structural interface — any class with score(self): i32 satisfies this
shape scorable
{
  score(self: self): i32;
}

task
{
  priority: i32;

  score(self: task): i32        // satisfies scorable — no declaration needed
  {
    self.priority
  }
}

bonus
{
  base: i32;
  multiplier: i32;

  score(self: bonus): i32       // also satisfies scorable
  {
    self.base * self.multiplier
  }
}

// T < scorable means T must satisfy the scorable shape (subtype constraint)
sum_scores[T](items: array[T]): i32 where T < scorable
{
  var total = 0;

  for items.values() item ->
  {
    total = total + item.score
  }

  total
}

main(): i32
{
  // Create an array of tasks and sum their scores
  let tasks = array[task]::fill(3);
  tasks(0) = task(10);
  tasks(1) = task(20);
  tasks(2) = task(30);

  let task_total = sum_scores(tasks);

  // Create an array of bonuses and sum their scores
  let bonuses = array[bonus]::fill(2);
  bonuses(0) = bonus(5, 2);
  bonuses(1) = bonus(3, 4);

  let bonus_total = sum_scores(bonuses);

  // task_total = 60, bonus_total = 22
  // Return combined total as exit code
  task_total + bonus_total
}
```

This returns exit code `82`. Key features:
- **Shapes**: `scorable` defines a structural interface — any class with `score(self): i32` satisfies it.
- **No `implements`**: `task` and `bonus` satisfy `scorable` by having matching methods — no explicit declaration needed.
- **Generic functions with constraints**: `sum_scores[T]` requires `T < scorable` — the type parameter must satisfy the shape.
- **Constructor sugar**: `task(10)` calls `task::create(10)` (auto-generated from the field definition). `bonus(5, 2)` calls `bonus::create(5, 2)`.
- **Type argument inference**: `sum_scores(tasks)` infers `T = task` from the argument.

---

## 1.6 Printing and Debugging Output

Verona is a research language and does not yet have a standard `print` function. For debugging, you can use two approaches:

### Exit Codes

The simplest way to observe program output is through the exit code of `main()`:

```bash
dist/vbci/vbci program.vbc
echo $?                               # prints the exit code
```

Many test programs use bitmask patterns to report multiple results through a single exit code.

### FFI `printval`

The interpreter provides a built-in `printval` function accessible via FFI. Declare it in a `use { ... }` block and call it with `:::` (the triple-colon operator, which invokes FFI functions and builtins):

```verona
use
{
  printval = "printval"(any): none;
}

main(): i32
{
  :::printval(42);
  :::printval("hello");
  0
}
```

`printval` prints the value to stdout followed by a newline. It accepts `any` type.

### Dumping Compiler Passes

To debug compilation issues, dump intermediate ASTs:

```bash
dist/vc/vc build ../my_project/ --dump_passes=./dump/
```

This creates one `.trieste` file per pass, letting you inspect the AST at each stage. You can also stop after a specific pass:

```bash
dist/vc/vc build ../my_project/ -p infer
```

See [Toolchain Usage](21-toolchain-usage.md) for all compiler and interpreter options.

---

## 1.7 What's Next

| Topic | Chapter |
|-------|---------|
| How programs and files are structured | [Program Structure](02-program-structure.md) |
| The type system | [Types](03-types.md) |
| Variables and bindings | [Declarations](04-declarations.md) |
| Defining classes | [Classes and Objects](08-classes-and-objects.md) |
| Writing functions | [Functions](07-functions.md) |
| All built-in types and their methods | [Built-in Types Reference](22-builtin-types.md) |
| Common gotchas for newcomers | [Gotchas and Pitfalls](26-gotchas.md) |
| Idiomatic patterns and recipes | [Common Patterns](27-common-patterns.md) |
| Compiler internals for contributors | [Compiler Pipeline](20-compiler-pipeline.md) |
