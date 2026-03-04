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
ninja update-dump-clean
ninja update-dump
cmake ..                              # re-run cmake to pick up new golden files
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
