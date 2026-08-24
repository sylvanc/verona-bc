# 21. Toolchain Usage

[← Table of Contents](README.md) | [Previous: Compiler Pipeline](20-compiler-pipeline.md) | [Next: Built-in Types →](22-builtin-types.md)

This chapter is a reference for the `vc` compiler and `vbci` interpreter command-line interfaces.

---

## 21.1 Compiling (`vc`)

### Basic Usage

```bash
vc build <source_dir>
```

Compiles all `.v` files in `<source_dir>` and produces `<dir_name>.vbc` in the current directory.

### Options

| Flag | Description |
|------|-------------|
| `-b <file>`, `--bytecode <file>` | Set the output bytecode filename |
| `-s`, `--strip` | Strip debug information from the bytecode |
| `-p <pass>`, `--pass <pass>` | Stop compilation after a specific pass |
| `--dump_passes=<dir>` | Dump intermediate ASTs to a directory |
| `-o <file>` | Output final AST (Trieste format) |

### Examples

```bash
# Compile with default output name
vc build my_project/                  # produces my_project.vbc

# Custom output
vc build my_project/ -b output.vbc

# Strip debug info
vc build my_project/ -s

# Stop after type inference (for debugging)
vc build my_project/ -p infer

# Dump all intermediate ASTs
vc build my_project/ --dump_passes=./dump/
```

### Output Naming

The output filename is derived from the source directory name:
- `vc build hello/` → `hello.vbc`
- `vc build my_project/` → `my_project.vbc`

Use `-b` to override.

### Important: Use the Installed Binary

Always use `build/dist/vc/vc`, not `build/vc/vc`. The installed binary has the `_builtin` standard library directory next to it, which the compiler requires for name resolution.

---

## 21.2 Running (`vbci`)

### Basic Usage

```bash
vbci <program.vbc>
```

Executes the bytecode file. The process exit code is the return value of `main()`.

### Options

| Flag | Description |
|------|-------------|
| `-t <N>`, `--threads <N>` | Number of scheduler threads (default: available CPU cores) |
| `-l <level>`, `--log_level <level>` | Set log level |

### Log Levels

`Trace`, `Debug`, `Info`, `Warning`, `Output`, `Error`, `None`

### Examples

```bash
# Run a program
vbci my_project.vbc

# Check the exit code
vbci my_project.vbc; echo $?

# Run with specific thread count
vbci my_project.vbc -t 4

# Debug logging
vbci my_project.vbc -l Debug
```

---

## 21.3 Development Workflow

### Build, Install, Test

```bash
cd build
ninja install                         # build and install to dist/
ctest --output-on-failure -j$(nproc)  # run test suite
```

### Compile and Run

```bash
cd build
dist/vc/vc build ../my_project/
dist/vbci/vbci my_project.vbc
echo $?                               # check exit code
```

### Debugging a Compilation Issue

```bash
# Dump ASTs to see where things go wrong
dist/vc/vc build ../my_project/ --dump_passes=./dump/

# Stop at a specific pass to inspect
dist/vc/vc build ../my_project/ -p ident

# Debug the interpreter with lldb
lldb-20 -- dist/vbci/vbci my_project.vbc
```

### Updating Test Golden Files

```bash
cd build
ninja update-dump
```

---

## 21.4 Test Conventions

Tests live in `testsuite/v/<name>/`:

```
testsuite/v/hello/
  hello.v                             # source file
  hello/compile/
    exit_code.txt                     # expected compile exit code (no trailing newline)
    stdout.txt                        # expected compile stdout (usually empty)
    stderr.txt                        # expected compile stderr (usually empty)
  hello/run/
    exit_code.txt                     # expected run exit code
    stdout.txt                        # expected run stdout
    stderr.txt                        # expected run stderr
```

Tests must be self-contained — no external dependencies. Use only `_builtin` types.

---

## 21.5 Understanding Compiler Errors

When compilation fails, `vc` prints errors with source locations and context. Here are common error patterns:

### Undefined Type or Identifier

```
Errors:
  Identifier not found
    -- main.v:3:11
      let p = unknown(42);
              ^~~~~~~
Pass ident failed with 1 error(s)!
```

**Cause:** Using a class, type, or qualified name that doesn't exist in scope. Check spelling and make sure the type is defined or imported via `use`.

### Unknown Method

```
Errors:
  unknown method ???
    -- main.v:5:13
      let b = a.nonexistent;
                ^~~~~~~~~~~
Pass typecheck failed with 1 error(s)!
```

**Cause:** Calling a method that isn't defined on the receiver type. The `???` indicates the method could not be resolved. Check the method name and that the receiver has the correct type.

### Wrong Number of Arguments

```
Errors:
  wrong number of arguments
    -- main.v:5:11
      let p = new { f = 42 }
              ^~~
Pass typecheck failed with 1 error(s)!
```

**Cause:** Calling a function or constructor with too many or too few arguments. For `new`, this usually means you're using `new` outside a class body. See [Classes §8.2](08-classes-and-objects.md).

### General Tips

- The error message names the **pass** that failed (`ident`, `typecheck`, etc.).
- Source locations are `file:line:column` with a caret (`^`) underlining the problematic token.
- Use `--dump_passes=./dump/` to inspect intermediate ASTs leading up to the failure.

---

## 21.6 Debugging Runtime Errors

When a program compiles successfully but produces a runtime error, the interpreter prints the error type and terminates the behavior (or the whole program). Here's how to diagnose common runtime errors:

### `bad type`

The most common runtime error. A value's actual type doesn't match the expected type at a function call, field store, or array store.

**How to diagnose:**
1. Compile with `--dump_passes=./dump/` and inspect the `infer` pass output. Look at the types assigned to `Const` nodes — are they what you expect?
2. Default literals (`42`, `3.14`) start as `u64`/`f64`. If inference can't determine the concrete type from context, they stay as defaults and may mismatch at runtime.
3. **Fix:** Add explicit type annotations on literals (`i32 42`) or on variables (`var x: i32 = 42`).

### `bad array index`

Array index out of bounds. Check that your loop counter stays within `0..arr.size`.

### `bad stack escape`

A stack-allocated value is escaping its frame — typically returned from a function or stored into a heap object. This usually means a primitive was expected but an object reference was used.

### `bad store`

A region invariant was violated. Common causes:
- Storing creates a cycle between regions (A references B and B references A)
- Storing a frame-local value into a frozen or read-only region
- Double-parenting: an object already has a region parent and you're trying to move it to another

### `bad operand`

An arithmetic or comparison operation received an invalid value — most often an uninitialized variable (see [Declarations §4.2](04-declarations.md)).

### General Debugging Strategy

1. **Check types first.** Most runtime errors trace back to a type mismatch. Use `--dump_passes` to inspect what types the compiler assigned.
2. **Use exit codes for testing.** Since `:::printval` is the only output mechanism, use the exit code to verify intermediate values: `main(): i32 { /* ... */; suspicious_value.i32 }`.
3. **Use `lldb-20` for crashes.** If the interpreter itself crashes (segfault), debug with `lldb-20 -- dist/vbci/vbci program.vbc`.
4. **Reduce to a minimal case.** Strip your program down until the error disappears, then add back until it reappears. The last addition is likely the cause.

---

## 21.7 Internal Subcommands

The `vc` compiler also provides `vc check` and `vc test` subcommands. These are internal debugging tools used during compiler development and are not intended for general use. They may change or be removed without notice.

---

## 21.8 Experimental LLVM Backend

The experimental LLVM backend lowers backend Trieste IR (`.vir`) directly to
textual LLVM IR (`.ll`). It is enabled by default, so the default configuration
requires an LLVM installation discoverable by CMake:

```bash
cmake -S . -B build -G Ninja \
  -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm
cd build
ninja install
```

To configure the project without LLVM, explicitly disable the backend:

```bash
cmake -S . -B build -G Ninja \
  -DVERONA_ENABLE_LLVM_BACKEND=OFF
```

The standalone `vbcc` selects its compiled output format explicitly:

| Flag | Description |
|------|-------------|
| `--emit <vbc\|llvm-ir>` | Select VBC or textual LLVM IR output; defaults to `vbc` |
| `--output-file <file>` | Set the compiled output path |

When `--output-file` is omitted, `vbcc` derives the filename from the input and
uses `.vbc` or `.ll` according to `--emit`. An explicit filename must use the
matching extension. The existing `-o`/`--output` option remains reserved for
the final Trieste AST rather than the compiled artifact.

Use the installed `vbcc` and select LLVM IR output:

```bash
dist/vbcc/vbcc build \
  ../testsuite/vir/simp1/simp1.vir \
  --emit llvm-ir \
  --output-file simp1.ll

llc -filetype=obj simp1.ll -o simp1.o
c++ simp1.o dist/lib/libvrt.a -o simp1
./simp1
echo $? # 0
```

`libvrt.a` supplies the native entry point and the process-local
`set_exit_code(i32)` FFI function used by this first backend slice.
The compiler/runtime boundary is declared by the installed, C-compatible VRT
headers. `<vrt/program.h>` declares `set_exit_code(int32_t)` as a
runtime-provided function and `verona_program_entry(void)` as the generated
program entry point. `<vrt/thread.h>` exposes logical-thread access, while
`<vrt/frame.h>` exposes logical-frame operations and generated function
descriptors. Both use the opaque types declared by `<vrt/types.h>`. Logical
frames form a parent chain, carry a stable runtime-assigned identity and
generated function descriptor, and can be rebound without changing identity
in preparation for a tailcall. Their concrete C++ layouts remain private to
`libvrt`.

`libvrt` binds one logical `vrt_thread` to each participating native thread
using thread-local storage. Runtime-owned startup and teardown perform that
binding; generated code does not create or destroy threads. Code that needs
the runtime thread can probe it with `vrt_thread_current()` instead of carrying
a hidden thread argument through every Verona call.

Internal Verona functions receive only their declared user parameters; runtime
context is not carried in hidden LLVM arguments. Every generated function
prologue calls `vrt_frame_enter`. For an ordinary call, it pushes a new logical
frame. After `vrt_frame_prepare_tailcall`, the next callee prologue consumes a
thread-local pending-transfer marker and reuses the prepared frame instead.
Generated returns call `vrt_frame_leave`, while a tailcall transfers the frame
without leaving it. The C-compatible `verona_program_entry` wrapper calls the
internal `@main` function without performing runtime setup or teardown.

A static VIR `call` resolves its `FunctionId` through the module's predeclared
function table, applies each argument's `ArgMove` or `ArgCopy` ownership
operation, and emits a direct LLVM call with the callee's Verona calling
convention. The callee's existing prologue and epilogue push and pop the
logical frame, so ordinary calls require no separate frame operation at the
call site.

Each generated function also saves a native `setjmp` continuation in its
logical frame. A VIR `raise` consumes its source value, encodes the currently
supported scalar or raw-pointer representation in a 64-bit runtime word, and
calls `vrt_frame_raise`. The runtime validates the active target, removes the
intermediate logical frames, and resumes the target continuation. That target
then consumes the payload, reconstructs its native return representation,
leaves its frame, and returns to its caller. Tailcalled functions overwrite the
continuation in the reused logical frame, so the stable frame identity still
names the current native activation.

VIR `getraise` and `setraise` are ordinary side-effecting statements around
that control transfer. `getraise` reads the current logical frame's target,
while `setraise` borrows a `u64` target, installs it, and returns the previous
target. Setting a target does not validate it; `raise` validates that the saved
identity still names an active ancestor when it performs the non-local return.

Frame entry and exit remain runtime calls in this implementation. They define
the semantic slow path that generated prologues and epilogues can later replace
with inline fast paths while retaining runtime fallbacks.

Before the LLVM `musttail` call, the backend transfers each `MoveArg` and calls
`vrt_frame_prepare_tailcall`. Static calls record generated function metadata;
the current raw-pointer dynamic representation records a null target, which the
actual tailcallee replaces with its descriptor when its prologue consumes the
transfer. The liveness pass expresses non-transferred register cleanup as
explicit `Drop` statements before the terminator.

Frame-local regions, stack-object finalization, and managed ownership
operations are not yet implemented by the native runtime. As those
representations are added, `vrt_frame_prepare_tailcall` will reset their
per-activation state while preserving the logical frame, its identity, and its
frame-local region.

> **Status:** The LLVM backend currently supports scalar primitive types,
> multi-block conditional control flow, scalar operations, copy/move/drop,
> static calls, process-local non-variadic FFI calls, returns,
> scalar/raw-pointer `raise` payloads, and static tailcalls. Dynamic tailcalls
> are supported when the target has the current raw `ptr` representation.
> Verona functions use LLVM `tailcc`; the exported C-compatible
> `verona_program_entry` wrapper enters the internal Verona calling convention.
> Managed runtime representations, dynamic calls, fallible dynamic calls, and
> dynamic lookup are not yet lowered, so source-level block-lambda raise is not
> yet available end to end through the native backend. Unsupported operations,
> library forms, symbol versions, and variadic calls produce an LLVM-backend
> diagnostic. A build configured without `VERONA_ENABLE_LLVM_BACKEND` similarly
> rejects `--emit llvm-ir`.
