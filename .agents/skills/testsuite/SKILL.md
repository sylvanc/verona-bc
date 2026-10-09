---
name: testsuite
description: Verona compiler test infrastructure covering VBC golden tests, LLVM-native tests, and libvrt ABI/runtime tests. Use when adding or registering tests, running focused or full CTest suites, updating golden files, debugging failures, checking exit codes, or understanding the testsuite CMake layout.
---

# Verona Test Suite Guide

## Architecture

`testsuite/CMakeLists.txt` assigns top-level collections to two suites:

```cmake
include("${trieste_SOURCE_DIR}/cmake/testsuite.cmake")
testsuite(
  compiler
  COLLECTIONS vc-vbc.cmake vc-llvm.cmake virc-vbc.cmake virc-llvm.cmake)
testsuite(runtime COLLECTIONS vrt.cmake)
```

`compiler` owns end-to-end source/VIR pipelines. `runtime` owns hand-written
C/C++ tests which call VRT directly. Collection ownership is explicit; helper
files under `testsuite/cmake/` or `testsuite/llvm/cmake/` are never collections.

The five collections are:

| Collection | Selected input | Registered graph |
|---|---|---|
| `vc-vbc.cmake` | `*.v` | Verona compile -> bytecode run |
| `vc-llvm.cmake` | eligible `*.v` | Verona compile -> emit IR -> assemble -> codegen -> link -> native run |
| `virc-vbc.cmake` | `*.vir` | VIR compile -> bytecode run |
| `virc-llvm.cmake` | eligible `*.vir` | emit IR -> assemble -> codegen -> link -> native run |
| `vrt.cmake` | selected C/C++ sources under `vrt/` | build libvrt test targets and register run nodes |

`testsuite/cmake/compiler_fixtures.cmake` is the authoritative feasibility
manifest. Defaults, anchored rules, explicit source groups, and optional exact
overrides produce effective `VBC_STAGE` and `LLVM_STAGE` values; collections
register all nodes through those terminal stages. The source basename must
match its parent directory for `.v` fixtures:

```text
testsuite/v/hello/hello.v
testsuite/vir/simp1/simp1.vir
```

Fixture names describe behavior, not backend. Compiler fixtures exercising
generated VRT behavior remain under `v/` or `vir/`; direct VRT fixtures remain
under `vrt/`. Read `testsuite/docs/architecture.md` for stage, label, and
runtime-boundary rules.

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
compiler/v/hello/hello/compile
compiler/v/hello/hello/run
compiler/vir/simp1/simp1/compile
compiler/vir/simp1/simp1/run
runtime/vrt/behavior/set-exit
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

The LLVM collections query `LLVM_STAGE`; a fixture reaching `run` has five
native nodes:

```text
emit-ir -> assemble -> codegen -> link -> run
   .ll        .bc        .o       executable
```

The emit node uses the installed compiler for its input language: `virc
--emit llvm-ir` for textual VIR or `vc --emit llvm-ir` for Verona source. Its
validator rejects an `.ll` file without both a target data layout and target
triple. `llvm-as` verifies and assembles the IR, `llc` emits the platform
object, and the C++ driver links it with installed `libvrt`.

Every stage commits only the three process-result goldens. `.ll`, `.bc`,
object, executable, and final-AST files remain transient artifacts under the
hashed build output. The golden layout for one fixture is:

```text
testsuite/vir/scalar_ops/scalar_ops/
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
When `VERONA_ENABLE_LLVM_BACKEND=OFF`, both LLVM collections select no files
but the other three collections continue to configure.

### libvrt

`vrt.cmake` creates ordinary CMake executable targets linked with `vbc::vrt`,
then registers their execution as named nodes:

- `vrt/abi/{c,cxx}` tests public C11/C++ layout, inclusion, linkage, and
  signatures;
- `vrt/api/{array,error,frame,function,object,program,thread}` tests one
  exported VRT API family per executable;
- `vrt/behavior/{default-exit,set-exit,last-write-wins}` tests generated
  program exit behavior with expected statuses `0`, `7`, and `3`;
- `vrt/internal/{collect,failure,finalizer,frame,freeze,location,region,scc,writebarrier}`
  tests collector queues, private diagnostics, finalizer cleanup, frame
  transitions, Freeze/SCC lifetime, tagged locations, region ownership, and
  write barriers.

The API fixtures are hand-written stand-ins for generated native code. They
call the same exported functions as generated code and provide the subset of
compiler-emitted metadata required by the API under test. Where needed, they
construct `vrt::Function`, `vrt::Field`, `vrt::Method`, `vrt::Class`,
`vrt::TypeInfo`, `vrt::Singleton`, and `vrt::Program` descriptors using the
same ABI layouts and relationships emitted by the LLVM backend. Numeric IDs,
field-storage structs, and sample values are deliberately small synthetic test
data.

The other fixture categories exercise different boundaries. ABI fixtures
check that public declarations and layouts are usable from C11 and C++.
Behavior fixtures provide the symbols normally supplied by generated code and
check process-level results. API and internal fixtures may include private VRT
headers to inspect headers, reference counts, regions, or thread state. Such
inspection is test-only: API fixtures still drive behavior through exported
VRT functions, while internal fixtures directly test runtime invariants.

The region test covers object and array allocation in frame-local regions,
existing RC/arena regions, and fresh RC/arena regions.

The collector-related internal fixtures divide ownership by abstraction:
`collect` covers queued two-phase reclamation, `finalizer` covers thunk and
cleanup ordering, `freeze` covers graph discovery and RC-to-ARC publication,
`scc` covers published-component lifetime, and `writebarrier` covers immutable
reference accounting and region-parent rejection.

Each VRT node is a self-contained fixture whose source name matches
its directory name and whose expected outputs are next to the source:

```text
testsuite/vrt/api/array/
├── array.cc
├── exit_code.txt
├── stderr.txt
└── stdout.txt
```

The fixture directory is also the node name (`vrt/api/array` in this example).
Unlike compiler pipelines, VRT tests have no separate compile and run nodes,
so they do not add a redundant `run/` level.

## Fixture Coverage Documentation

Every non-trivial LLVM or VRT fixture must document the boundary it verifies.
Keep a short explanation at the top of the fixture source when it fits without
obscuring the test. For LLVM-native fixtures, prefer this structure:

```text
Coverage
Native VRT coverage
Non-goals
```

- **Coverage** names the compiler constructs, emitted metadata, or runtime
  operations that the fixture directly exercises.
- **Native VRT coverage** names the generated metadata or calls that VRT
  actually consumes. Do not claim that a successful link or execution proves
  a descriptor was consumed when the program only emitted it.
- **Non-goals** records adjacent behavior intentionally left to another
  fixture, such as allocation, singleton initialization, or dynamic dispatch.

Adapt the runtime heading when appropriate, for example `VBCI coverage` or
`Runtime coverage`. Very small regression fixtures may use a compact paragraph
instead of all three headings, but the verified behavior must remain explicit.

For a long fixture or one covering several distinct scenarios, put the detailed
explanation in an adjacent `README.md` rather than adding a large comment block
that pushes the test body out of view. Keep a one- or two-line source header
pointing to that README. Place it beside the source, for example:

```text
testsuite/vir/object_alloc/
├── README.md
├── object_alloc.vir
└── object_alloc/
    └── ... goldens ...
```

The README should summarize the fixture's purpose, exercised pipelines, major
scenarios, runtime boundary, and non-goals. It should not duplicate a
line-by-line walkthrough of the source or live inside a generated golden
directory.

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
ctest --output-on-failure -R '^compiler/v/hello/hello/'

# One VIR fixture through bytecode
ctest --output-on-failure -R '^compiler/vir/simp1/simp1/(compile|run)$'

# All bytecode and native nodes for one LLVM fixture
ctest --output-on-failure \
  -R '^compiler/vir/scalar_ops/scalar_ops/'

# Only that fixture's native LLVM stages
ctest --output-on-failure \
  -R '^compiler/vir/scalar_ops/scalar_ops/llvm/'

# All VRT nodes
ctest --output-on-failure -R '^runtime/vrt/'

# One frontend/backend intersection
ctest --output-on-failure \
  -L '^frontend:virc$' -L '^backend:llvm$'

# Direct and compiler-generated VRT coverage
ctest --output-on-failure -L '^runtime:vrt$'

# List registered tests
ctest -N
```

Suite labels describe ownership. Derived `frontend:*` and `backend:*` labels
describe compiler nodes; `runtime:vrt` selects direct and intentional
compiler-generated VRT coverage. Labels never configure eligibility.

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
  even if `virc` returned zero.
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

1. Every collection listed in `testsuite/CMakeLists.txt` must define
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
8. Check that defaults and structural rules give each new fixture the intended
  stages; use a source group for shared explicit metadata and an exact fixture
  override for one-off behavior.
9. Keep compiler integration fixtures under `v/` or `vir/` even when they
  exercise VRT; use `runtime:vrt` and coverage text to identify that purpose.
10. Name fixtures by behavior. Do not encode backend eligibility in prefixes.
