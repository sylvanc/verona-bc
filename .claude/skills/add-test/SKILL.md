---
name: add-test
description: Create a new Verona compiler test case. Use when the user wants to add a test, create a test, or scaffold a test for the compiler.
argument-hint: <test-name> [error]
---

# Add a Verona Compiler Test

Create a new test case in `testsuite/v/` for the Verona compiler.

## Arguments

- `$0` — the test name (required, snake_case)
- `$1` — if set to `error`, creates a compile-error test (exit code 1, no run/ directory)

## Steps

### 1. Create the source file

Create `testsuite/v/$0/$0.v` with appropriate Verona source code.

**Conventions:**
- Tests must be self-contained — no external dependencies (no `use "https://..."`)
- Use only `_builtin` types
- Do NOT write `use "_builtin"` — it is always implicitly available
- Do NOT call FFI functions
- No semicolons after closing braces of control flow (`for`, `if`, `while`)
- Semicolons are needed after statements and field definitions
- Don't use explicit type annotations on literals when inference can determine the type from context (write `0` not `usize 0`)
- Class definitions use bare names: `myclass[T] { ... }`
- Fields need semicolons: `val: T;`
- `new` uses `new { field = val }` (no class name after `new`)

**For success tests (default):**
- Use the bitmask exit code pattern: `var result = 0;` then `if cond { result = result + N; }` with powers of 2
- Exit code 0 means all checks passed
- The `main` function must return `i32`

**For compile-error tests (`$1` = `error`):**
- Write code that should fail to compile
- The test validates that the compiler correctly rejects invalid code

### 2. Generate golden files

Run from the `build` directory:

```bash
cd build
ninja update-dump-clean && ninja update-dump && cmake ..
```

This auto-generates the golden file directory structure:
- `testsuite/v/$0/$0/compile/` — contains `exit_code.txt`, `stdout.txt`, `stderr.txt`, pass dumps, and `.vbc` file
- `testsuite/v/$0/$0/run/` — contains `exit_code.txt`, `stdout.txt`, `stderr.txt` (only for success tests)

### 3. Verify the test passes

```bash
cd build && ctest --output-on-failure -R "^vbc/$0" -j$(nproc)
```

This should show the test passing. If it fails, check the source code and re-run `ninja update-dump`.

### 4. Report

State the test name, whether it's a success or error test, what it tests, and the expected exit code.
