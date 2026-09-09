---
name: testsuite
description: Verona compiler test infrastructure covering VBC golden tests, LLVM-native tests, and libvrt ABI/runtime tests. Use when adding or registering tests, running focused or full CTest suites, updating golden files, debugging failures, checking exit codes, or understanding the testsuite CMake layout.
---

# Verona Test Suite Guide

## Architecture

`testsuite/CMakeLists.txt` includes Trieste's named-node runner and calls one
entry point:

```cmake
include("${trieste_SOURCE_DIR}/cmake/testsuite.cmake")
testsuite(vbc)
```

The runner treats every `*.cmake` file directly under `testsuite/` as a test
collection. Keep helpers in component subdirectories such as
`testsuite/llvm/cmake/`; an adjacent helper would be mistaken for a
collection.

The four collections are:

| Collection | Selected input | Registered graph |
|---|---|---|
| `vc.cmake` | `*.v` | Verona compile -> bytecode run |
| `vbc.cmake` | `*.vir` | VIR compile -> bytecode run |
| `llvm.cmake` | `vir/llvm_*/*.vir` | emit IR -> assemble -> codegen -> link -> native run |
| `vrt.cmake` | the four sources under `vrt/` | build C/C++ targets and register six run nodes |

`vc.cmake` and `vbc.cmake` omit the run node for sources below a
`compile_only/` directory. The source basename must match its parent
directory, so a normal fixture has one clear root:

```text
testsuite/v/hello/hello.v
testsuite/vir/simp1/simp1.vir
```

The `llvm_*` files remain ordinary VIR fixtures. Each is compiled and run as
bytecode by `vbc.cmake` and also follows the native LLVM graph registered by
`llvm.cmake`. There is no duplicate LLVM source tree.

## Named Nodes

A collection sets `TESTSUITE_REGEX`, sets `TESTSUITE_DEFINE` to a callback,
and calls `testsuite_add_test()` from that callback. A node declares:

- a suite-local `NAME`;
- a `WORKING_DIRECTORY` and final `COMMAND`;
- committed `GOLDENS` such as `exit_code.txt`, `stdout.txt`, and
  `stderr.txt`;
- transient `ARTIFACTS` that later nodes consume;
- optional `DEPENDS`, `TIMEOUT`, and `VALIDATOR` metadata.

Use `testsuite_output_path()` to obtain the deterministic build-tree path of
an artifact produced by another node. Do not construct or depend on the
runner's SHA-256 output directory directly.

For every node, Trieste generates one private configuration file during CMake
configuration. CTest and `ninja update-dump` both pass that file to
`execute_test_node.cmake`, so verification and golden updates run the same
command and validator.

Each named node is one CTest test. Trieste fixtures make a selected dependent
node execute its prerequisites and prevent downstream work after a failed
node. The public CTest name is the suite name plus the node name, for example:

```text
vbc/v/hello/hello/compile
vbc/v/hello/hello/run
vbc/vir/simp1/simp1/compile
vbc/vir/simp1/simp1/run
vbc/vrt/behavior/set-exit/run
```

## Pipelines

### Verona and VIR bytecode

The compile node writes a `.vbc` artifact into its private build-tree output
directory. The run node depends on compile and passes that exact artifact to
the installed `vbci`:

```text
source -> compile -> .vbc -> run
```

The final `.trieste` AST and `.vbc` are transient build artifacts. They are
not copied into the source tree as goldens. Pass dumps are produced only when
`--dump_passes` is explicitly requested for diagnosis.

### LLVM native

`llvm.cmake` selects only fixtures named `vir/llvm_*/llvm_*.vir`. Each fixture
has five nodes:

```text
emit-ir -> assemble -> codegen -> link -> run
   .ll        .bc        .o       executable
```

The emit node uses installed `vbcc --emit llvm-ir`. Its validator rejects an
`.ll` file without both a target data layout and target triple. `llvm-as`
verifies and assembles the IR, `llc` emits the platform object, and the C++
driver links it with installed `libvrt`.

Every stage commits only the three process-result goldens. `.ll`, `.bc`,
object, executable, and final-AST files remain transient artifacts under the
hashed build output. The golden layout for one fixture is:

```text
testsuite/vir/llvm_scalar_ops/llvm_scalar_ops/
├── compile/                 # ordinary bytecode compile
├── run/                     # ordinary bytecode run
└── llvm/
    ├── emit-ir/
    ├── assemble/
    ├── codegen/
    ├── link/
    └── run/
```

Each leaf directory contains `exit_code.txt`, `stdout.txt`, and `stderr.txt`.
When `VERONA_ENABLE_LLVM_BACKEND=OFF`, the LLVM collection selects no files
but the other three collections continue to configure.

### libvrt

`vrt.cmake` creates ordinary CMake executable targets linked with `vbc::vrt`,
then registers their execution as named nodes:

- `vrt/abi/c/run` tests the public C11 ABI;
- `vrt/abi/cxx/run` tests C++ inclusion and C-linkage signatures;
- `vrt/behavior/default-exit/run` expects exit `0`;
- `vrt/behavior/set-exit/run` expects exit `7`;
- `vrt/behavior/last-write-wins/run` expects exit `3`;
- `vrt/internal/state/run` tests private runtime state transitions.

The named executor records the process's exact numeric exit status, so the
nonzero behavioral expectations need no `WILL_FAIL` property or wrapper
script. VRT nodes use the same three committed golden files as every other
node.

## Source Goldens and Build Artifacts

For a standard fixture, committed goldens are colocated with the source:

```text
testsuite/{v,vir}/<name>/<name>/compile/
├── exit_code.txt
├── stderr.txt
└── stdout.txt

testsuite/{v,vir}/<name>/<name>/run/
├── exit_code.txt
├── stderr.txt
└── stdout.txt
```

`exit_code.txt` is a plain number with no trailing newline. Compiler-error
fixtures belong under `compile_only/`, normally have compile exit `1`, and
have no run directory.

Actual outputs and artifacts live below
`build/testsuite/testsuite-output/<suite-hash>/<node-hash>/`. The hashes
prevent logical node names such as `foo` and `foo/bar` from sharing physical
directories. Use node names and `testsuite_output_path()`, not these hashes,
when working with the graph.

All declared goldens are compared with `cmake -E compare_files --ignore-eol`.
This means stderr and stdout content are validated, not only the exit code.
All declared artifacts must exist before a node is accepted.

## Running Tests

Always build and install first so compiler tests use binaries below
`build/dist/`; only the installed `vc` has `_builtin` beside it.

```bash
cd build
ninja install
ctest --output-on-failure -j$(nproc)
```

Useful focused commands:

```bash
# One Verona fixture: compile and run
ctest --output-on-failure -R '^vbc/v/hello/hello/'

# One VIR fixture through bytecode
ctest --output-on-failure -R '^vbc/vir/simp1/simp1/(compile|run)$'

# All bytecode and native nodes for one LLVM fixture
ctest --output-on-failure \
  -R '^vbc/vir/llvm_scalar_ops/llvm_scalar_ops/'

# Only that fixture's native LLVM stages
ctest --output-on-failure \
  -R '^vbc/vir/llvm_scalar_ops/llvm_scalar_ops/llvm/'

# All VRT nodes
ctest --output-on-failure -R '^vbc/vrt/'

# List registered tests
ctest -N
```

All collections are currently in the `vbc` suite and therefore have the
`vbc` CTest label. Select LLVM and VRT subsets by their logical name prefixes,
not by `-L llvm` or `-L vrt`.

After adding a fixture or changing CMake registration, run `ninja install`
or `cmake ..` so CMake's configured globs refresh the registry.

## Updating Goldens

Run:

```bash
cd build
ninja install
ninja update-dump
```

The update target follows the same dependency graph as CTest. It executes a
producer before its consumers and builds CMake targets referenced by node
commands. It copies only files declared in `GOLDENS`; transient artifacts are
never committed.

After updating, run `ninja update-dump` a second time and inspect `git status`.
The second run should not change tracked files. Then run the focused and full
CTest suites.

Adding or changing a file in `vc/_builtin/` affects every Verona compile
because `_builtin` is implicitly parsed. Regenerate and review the complete
golden set in that case.

## Diagnosing Failures

- A dependent node automatically pulls in its prerequisite nodes. Fix the
  first failing stage in the chain.
- Missing artifact errors mean the command exited but did not produce a file
  listed in `ARTIFACTS`.
- Missing golden errors mean `ninja update-dump` has not generated the
  declared source result.
- An LLVM emit validator failure means the `.ll` is missing target metadata,
  even if `vbcc` returned zero.
- Timeout or signal results are rejected because the executor requires a
  numeric process exit code.
- Do not use `WILL_FAIL` for an exact nonzero expectation; commit that number
  in the node's `exit_code.txt`.

Generate pass dumps manually when investigating compiler stages:

```bash
cd build
dist/vc/vc build ../testsuite/v/<name> --dump_passes=dump_<name>
```

Run `vc` from `build/` with a source-directory argument. Running `vc build .`
inside a source directory can derive a hidden output name from `.`.

## Collection Rules and Pitfalls

1. Every `*.cmake` file directly under `testsuite/` must define
   `TESTSUITE_REGEX` and a callable `TESTSUITE_DEFINE`.
2. Keep validators and other helper scripts below a component subdirectory.
3. Node names, dependency names, golden paths, and artifact paths must be
   relative and stable; do not put generator expressions in graph identity.
4. Put `COMMAND` last in `testsuite_add_test()` because all following
   arguments are treated as the opaque command argv.
5. Use `testsuite_output_path()` for every cross-node artifact reference.
6. Declare `exit_code.txt` in every `GOLDENS` list.
7. Do not commit `.vbc`, `.ll`, `.bc`, `.o`, executables, or final/pass AST
   files as runner goldens.
8. Keep LLVM-capable `.vir` sources under `testsuite/vir/` so the ordinary
   bytecode collection continues to test them too.
