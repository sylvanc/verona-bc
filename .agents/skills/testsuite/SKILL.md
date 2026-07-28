---
name: testsuite
description: Verona compiler test infrastructure covering VBC golden tests, LLVM-native tests, and libvrt ABI/runtime tests. Use when adding or registering tests, running focused or full CTest suites, updating golden files, debugging failures, checking exit codes, or understanding the testsuite CMake layout.
---

# Verona Test Suite Guide

## Test Infrastructure Overview

Top-level CMake calls `enable_testing()` and adds `testsuite/`.
`testsuite/CMakeLists.txt` includes collection-level registration modules and
writes the CTest registry under `build/testsuite/CTestTestfile.cmake`. Run
CTest from `build/`; CTest names are logical identifiers, not source paths.

The current layout is:

```text
testsuite/
├── CMakeLists.txt
├── verona_testsuite.cmake       # Generic Trieste-based golden harness
├── vbc_tests.cmake              # Registers vc, vbcc, and vbci collections
├── vc.cmake
├── vbcc.cmake
├── vbci.cmake
├── llvm_tests.cmake             # Registers optional LLVM-native tests
├── llvm_native_test.cmake       # LLVM-native execution helper
├── vrt_tests.cmake              # Registers libvrt tests
├── vrt/
│   ├── abi_c.c
│   ├── abi_cxx.cc
│   ├── exit_code_program.c
│   ├── runtime_state.cc
│   └── cmake/
│       └── check_exit_code.cmake
├── v/                           # Verona source fixtures and goldens
└── vir/                         # Backend-IR fixtures and goldens
```

`testsuite/CMakeLists.txt` registers three paths:

- `vbc_tests.cmake` invokes the generic harness for VBC/VBCI tests.
- `vrt_tests.cmake` directly registers C, C++, behavioural, and private
  runtime tests linked with `vbc::vrt`.
- `llvm_tests.cmake` registers LLVM-native tests when
  `VERONA_ENABLE_LLVM_BACKEND` is enabled.

Keep collection-level registration modules in `testsuite/`. Keep a helper
used by only one suite with that suite, such as
`testsuite/vrt/cmake/check_exit_code.cmake`. Promote a helper to a shared
directory only after another suite reuses it.

The VBC path passes three explicit collection files to the generic harness:

| Layer | File | Tests | Input | Output dir |
|-------|------|-------|-------|------------|
| **vc** (compile) | `testsuite/vc.cmake` | Verona → bytecode | `*.v` files | `compile/` |
| **vbcc** (backend) | `testsuite/vbcc.cmake` | IR → bytecode | `*.vir` files | `compile/` |
| **vbci** (runtime) | `testsuite/vbci.cmake` | Execute bytecode | generated sibling `compile/*.vbc` | `run/` |

Backend-neutral `.vir` fixtures live under `testsuite/vir/`. A fixture can be registered for LLVM-native execution without duplicating its source.

The VBC harness records `exit_code.txt`, `stdout.txt`, and `stderr.txt`. A successful `vc` or `vbcc` compile also leaves generated `*_final.trieste` and `.vbc` artifacts in the build tree. Per-pass `.trieste` files are produced only when explicitly using `--dump_passes`.

The LLVM-native helper reuses a registered `.vir` fixture and writes `.ll`,
`.bc`, an object file, and a native executable under
`build/testsuite/llvm/<name>/`. It reports failure directly to CTest and does
not use source-tree golden files.

## Running Tests

### Full test suite

```bash
cd build
ninja install
ctest --output-on-failure -j$(nproc)
```

### Run specific test(s) by name

```bash
cd build && ctest --output-on-failure -R "^(v|vir)/<name>" -j$(nproc)
```

For example, `^v/hello/` matches both compile and run tests for the `hello` Verona source testcase. `^vir/simp1/` does the same for the `simp1` backend-IR testcase.

An LLVM registration for the same fixture has its own logical name:

```bash
cd build && ctest --output-on-failure -R "^llvm/simp1/native$"
```

### Run one backend path

```bash
cd build && ctest --output-on-failure -L vbc -j$(nproc)
cd build && ctest --output-on-failure -L llvm -j$(nproc)
cd build && ctest --output-on-failure -L vrt -j$(nproc)
```

The `vbc` label covers `vc`, `vbcc`, and `vbci`. The `llvm` label covers LLVM
IR generation, native compilation, linking, and execution. The `vrt` label
covers public ABI conformance, public behaviour, and private runtime state.

Run one `vrt` category by logical name:

```bash
cd build && ctest --output-on-failure -R "^vrt/abi/"
cd build && ctest --output-on-failure -R "^vrt/behavior/"
cd build && ctest --output-on-failure -R "^vrt/internal/"
```

### Run only compile or only runtime tests

```bash
cd build && ctest --output-on-failure -R "compile" -j$(nproc)   # all compile tests
cd build && ctest --output-on-failure -R "run" -j$(nproc)       # all runtime tests
```

### List available tests without running

```bash
cd build && ctest -N
cd build && ctest -N -L vrt
```

Run `cmake ..` from `build/` after changing CMake registration or adding a
test so CTest regenerates its registry.

## Source Fixtures, Golden Files, and Build Artifacts

### VBC source fixtures and golden files

Source fixtures are committed under:

```text
testsuite/v/<name>/<name>.v
testsuite/vir/<name>/<name>.vir
```

The source tree stores the committed expected results:

```text
testsuite/{v,vir}/<name>/<name>/compile/
├── exit_code.txt
├── stdout.txt
└── stderr.txt

testsuite/{v,vir}/<name>/<name>/run/    # successful runnable tests only
├── exit_code.txt
├── stdout.txt
└── stderr.txt
```

The `compile/` and `run/` are independent phases. For each phase, the VBC 
harness compares generated files under `build/testsuite/.../{compile,run}/` 
with the corresponding committed golden files under `testsuite/.../{compile,run}/`.

`exit_code.txt` has no trailing newline. A compile-error testcase has compile
goldens with exit code `1` and no `run/` golden directory; compilation also
produces no `.vbc` artifact.

### VBC generated build artifacts

CTest writes the generated actual results under matching phase paths:

```text
build/testsuite/{v,vir}/<name>/<name>/compile/
├── <name>_final.trieste
├── <name>.vbc             # successful compile only
├── exit_code.txt
├── stdout.txt
└── stderr.txt

build/testsuite/{v,vir}/<name>/<name>/run/
├── exit_code.txt
├── stdout.txt
└── stderr.txt
```

The generated `.trieste` and `.vbc` files are build artifacts, not committed
golden files. `vbci` executes the generated `.vbc` from the sibling `compile/`
directory.

### LLVM source fixtures and build artifacts

LLVM tests reuse explicitly registered fixtures from `testsuite/vir/`. For example:

```text
testsuite/vir/simp1/simp1.vir
```

The corresponding generated artifacts are separate from the VBC outputs:

```text
build/testsuite/llvm/simp1/
├── simp1.ll
├── simp1.bc
├── simp1.o                # platform-specific object suffix
└── simp1                  # platform-specific executable suffix
```

The expected native exit code is supplied by `llvm_tests.cmake`; there is no
separate `testsuite/llvm/` source fixture or LLVM golden directory.

### libvrt tests

`vrt` tests are ordinary compiled CTest targets; they do not use VBC golden
files:

- `vrt/abi/c` compiles the public `<vrt/abi.h>` interface as C11.
- `vrt/abi/cxx` compiles it as C++ and checks the C-linkage signatures.
- `vrt/behavior/default-exit`, `set-exit`, and `last-write-wins` link
  against `vbc::vrt` and exercise the public C ABI through the real native
  entry point.
- `vrt/internal/state` includes private `runtime/vrt.h` and directly
  checks runtime state transitions.

The behavioural programs intentionally return values such as `7` and `3`.
CTest normally treats every non-zero result as failure, while `WILL_FAIL`
accepts any non-zero value. Therefore,
`vrt/cmake/check_exit_code.cmake` runs each program and checks the exact
expected code. Keep this black-box check even when internal state tests cover
the same setters and getters.

### Regenerating golden files

Regenerate golden files only for the VBC collections. LLVM-native and `vrt`
tests do not have source-tree goldens.

When VBC output changes, regenerate the golden files:

```bash
cd build
ninja install && ninja update-dump-clean && ninja update-dump && cmake ..
```

- `ninja install` — rebuild the compiler (needed so installed binary has `_builtin`)
- `ninja update-dump-clean` — removes ALL existing golden directories, then runs update-dump
- `ninja update-dump` — regenerates golden output by running each test and copying results
- `cmake ..` — re-scans for new golden files so ctest knows about them

**IMPORTANT**: `update-dump-clean` removes golden dirs first, which is necessary when passes change (otherwise stale files remain). Use `update-dump` alone (without clean) only when you're sure no files were removed.

### When ALL golden files change

Adding or modifying a `.v` file under `vc/_builtin/` changes compilation output for EVERY test, because `_builtin` is always parsed. In this case, you MUST regenerate ALL golden files:

```bash
cd build && ninja install && ninja update-dump-clean && ninja update-dump && cmake ..
```

This can take a while. All golden file changes must be committed.

## Verifying Golden File Correctness

### Inspecting pass output

Pass dumps are diagnostic build artifacts, not golden files. Generate them explicitly when investigating a compiler pass:

```bash
cd build && dist/vc/vc build ../testsuite/v/<name> --dump_passes=dump_<name>
```

If dumping stops early, inspect the pass after the final emitted `.trieste`
file. Common causes are missing tokens in a pass WF or a mismatch between one
pass's output and the next pass's expected WF.

### Exit code verification

- `exit_code.txt` contains the exit code as a plain number with NO trailing newline
- The file is generated by cmake's `file(WRITE ...)` which doesn't append a newline
- Success compile tests: `0`
- Error compile tests: `1`
- Runtime tests: depends on what `main` returns (typically `0` for passing tests)

### Checking for correct error codes

For compile-error tests, verify:
1. `compile/exit_code.txt` contains `1`
2. No `run/` directory exists (compilation failed, nothing to run)
3. No `.vbc` file in `compile/` (compilation didn't produce output)
4. `stderr.txt` may contain error messages (but the framework doesn't validate error messages, only exit codes)

For runtime tests with expected non-zero exit codes:
1. `run/exit_code.txt` contains the expected value
2. The bitmask pattern (`result + 1`, `result + 2`, `result + 4`, ...) helps diagnose which checks failed

## How the Framework Works

### Test execution flow

For the VBC collections:

1. `ctest` runs Trieste's `runcommand.cmake` for each execution test.
2. `runcommand.cmake` includes the appropriate collection file, such as `vc.cmake`.
3. The collection's `toolinvoke` macro sets up command-line arguments.
4. The tool runs and records stdout, stderr, and its exit code to `stdout.txt`,
   `stderr.txt`, and `exit_code.txt`.
5. Separate CTest comparison entries compare those files with source goldens.
6. Timeout: 20 seconds per test

For the LLVM-native tests:

CTest runs `llvm_native_test.cmake`, which invokes
`vbcc`, `llvm-as`, `llc`, the native linker, and the resulting executable in
sequence. Any failed command or unexpected native exit code fails that single
CTest entry.

For the `vrt` tests:

1. CMake builds dedicated C or C++ executables linked with `vbc::vrt`.
2. ABI and internal tests run directly.
3. Behavioural tests run through `vrt/cmake/*.cmake`.
4. The helper fails unless the process returns the exact expected code.

### Golden file comparison

1. For every committed result file under
   `testsuite/{v,vir}/<name>/<name>/<phase>/`, find the generated file at the
   same relative path under `build/testsuite/`.
2. Compare `compile/` only with `compile/`, and `run/` only with `run/`.
3. Use `cmake -E compare_files --ignore-eol` so line-ending differences do
   not matter.
4. Fail the comparison test when the files differ.
5. Always check `exit_code.txt`, even when no other golden files exist.

### Test naming convention

CTest names are logical identifiers. They resemble paths but are not files or
directories under `testsuite/`.

VBC execution entries are named `v/<name>/<name>/compile`,
`v/<name>/<name>/run`, `vir/<name>/<name>/compile`, and
`vir/<name>/<name>/run`. Comparison entries append `-<filename>` to the
execution name. For example:

```text
CTest name:  vir/simp1/simp1/compile-exit_code.txt
Golden file: testsuite/vir/simp1/simp1/compile/exit_code.txt
Actual file: build/testsuite/vir/simp1/simp1/compile/exit_code.txt
```

LLVM-native CTest entries are named `llvm/<name>/native`. For example,
`llvm/simp1/native` reuses `testsuite/vir/simp1/simp1.vir` and writes artifacts
to `build/testsuite/llvm/simp1/`; `native` is a test-name component, not an
artifact subdirectory.

libvrt entries are grouped as `vrt/abi/<name>`, `vrt/behavior/<name>`, and
`vrt/internal/<name>`.

The test that generates output must pass before comparison tests run (set via `DEPENDS` property).

## Common Pitfalls

1. **Forgot `ninja install`**: The build binary at `build/vc/vc` does NOT have `_builtin` next to it. Only `build/dist/vc/vc` does. Always `ninja install` before `update-dump`.

2. **Forgot `cmake ..`**: After creating a new test directory, `cmake ..` must be run so ctest discovers the new golden files. Otherwise ctest won't know about the new comparison tests.

3. **Stale golden files**: If a pass was removed or renamed, `update-dump` won't delete old golden files. Use `update-dump-clean` to start fresh.

4. **Hidden `.vbc` file**: Running `vc build .` from inside a test directory produces `.vbc` (hidden file) because the project name is ".". Always run from `build/`: `dist/vc/vc build ../testsuite/v/<name>`.

5. **Timeout failures**: Tests that hang (infinite loops, deadlocks) are killed after 20 seconds and produce a non-zero exit code. The `exit_code.txt` will contain a timeout error string, not a number.

6. **New `_builtin` files**: Changes to `vc/_builtin/` affect ALL tests. Must regenerate all golden files and commit all changes.

7. **Missing pass dumps**: Usually a WF violation. Check which pass is the last one that produced output and investigate the next pass's WF definition.

8. **Using `WILL_FAIL` for an exact exit code**: `WILL_FAIL` accepts every
   non-zero result. Use the suite-local exact-exit helper for black-box
   behavioural tests.

9. **Putting suite helpers beside collection modules**: Keep
   `*_tests.cmake` registration modules in `testsuite/`; keep suite-specific
   execution helpers under the suite's own `cmake/` directory.
