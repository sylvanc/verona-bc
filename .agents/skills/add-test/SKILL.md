---
name: add-test
description: Create a new Verona compiler test case. Use when the user wants to add a test, create a test, or scaffold a test for the compiler.
argument-hint: <test-name> [error|ffi]
---

# Add a Verona Compiler Test

Create a new test case in `testsuite/v/` for the Verona compiler.

## Arguments

- `$0` — the test name (required, snake_case)
- `$1` — if set to `error`, creates a compile-error test (exit code 1, no run/ directory). If set to `ffi`, creates a test that uses FFI builtins.

## Steps

### 1. Create the source file

Create `testsuite/v/$0/$0.v` with appropriate Verona source code.

**Conventions:**
- Tests must be self-contained — no external dependencies (no `use "https://..."`)
- Use only `_builtin` types
- Do NOT write `use "_builtin"` — it is always implicitly available
- No semicolons after closing braces of control flow (`for`, `if`, `while`)
- Semicolons are needed after statements and field definitions
- Don't use explicit type annotations on literals when inference can determine the type from context (write `0` not `usize 0`)
- Class definitions use bare names: `myclass[T] { ... }`
- Fields need semicolons: `val: T;`
- `new` uses `new { field = val }` (no class name after `new`)
- Use `Type(args)` constructor sugar: `callback(f)` instead of `callback::create(f)`
- Use `(obj.field)(args)` for apply on field access, `x.method` for zero-arg methods

**For success tests (default):**
- Use the bitmask exit code pattern: `var result = 0;` then `if cond { result = result + N; }` with powers of 2
- Exit code 0 means all checks passed
- The `main` function must return `i32`

**For compile-error tests (`$1` = `error`):**
- Write code that should fail to compile
- The test validates that the compiler correctly rejects invalid code

**For FFI tests (`$1` = `ffi`):**
- Tests that use FFI builtins (`:::name(...)` syntax) or `_builtin/ffi/` wrappers (e.g., `ffi::add_external()`)
- If the test relies on init functions running, it MUST include at least one reachable FFI call from the same lib — init functions are only reified when their lib's FFI symbols are called from reachable code
- Declare FFI symbols in a `use` block with a `Lib` string
- Call FFI wrappers as `ffi::func_name(args)` for functions in `_builtin/ffi/`

### 2. Generate golden files

Run from the `build` directory:

```bash
cd build
ninja install && ninja update-dump-clean && ninja update-dump && cmake ..
```

This auto-generates the golden file directory structure:
- `testsuite/v/$0/$0/compile/` — contains `exit_code.txt`, `stdout.txt`, `stderr.txt`, pass dump files (`00_parse.trieste` through `13_typecheck.trieste`), `*_final.trieste`, and `.vbc` file (success tests only)
- `testsuite/v/$0/$0/run/` — contains `exit_code.txt`, `stdout.txt`, `stderr.txt` (only for success tests that produce a `.vbc`)

### 3. Verify golden file completeness

Check that the golden `compile/` directory has the expected files:
- All 14 pass dumps: `00_parse.trieste` through `13_typecheck.trieste`
- `*_final.trieste`
- `exit_code.txt` (value `0` for success, `1` for error, no trailing newline)
- `stdout.txt` and `stderr.txt`
- `*.vbc` file (success tests only)

If pass dumps are missing (e.g., only 0–5 present), it usually means a WF violation in a later pass. Investigate with `dist/vc/vc build ../testsuite/v/$0 --dump_passes=dump_$0` to see which pass fails.

For error tests: no `run/` directory, no `.vbc` file, `exit_code.txt` = `1`.

### 4. Verify the test passes

```bash
cd build && ctest --output-on-failure -R "^vbc/$0" -j$(nproc)
```

This should show the test passing. If it fails, check the source code and re-run `ninja update-dump`.

### 5. Report

State the test name, whether it's a success/error/ffi test, what it tests, and the expected exit code.
