# Testsuite Architecture

## Ownership Layers

The testsuite separates four concepts:

1. A **suite** owns a public CTest namespace, automatic label, and update target.
2. A **collection** owns one frontend/backend pipeline and registers its node graph.
3. A **fixture** is one canonical `.v` or `.vir` source, or one direct VRT source.
4. A **node** is one executable stage with exact goldens, artifacts, and dependencies.

`testsuite/CMakeLists.txt` assigns collection ownership explicitly:

```cmake
testsuite(
  compiler
  COLLECTIONS
    vc-vbc.cmake
    vc-llvm.cmake
    virc-vbc.cmake
    virc-llvm.cmake)

testsuite(runtime COLLECTIONS vrt.cmake)
```

The compiler suite owns end-to-end pipelines beginning with Verona source or
textual VIR. The runtime suite owns hand-written C/C++ fixtures which call VRT
directly and intentionally bypass compiler-generated code and metadata.

## Compiler Fixture Metadata

`cmake/compiler_fixtures.cmake` is the authoritative feasibility manifest for
canonical sources under `v/` and `vir/`. Effective metadata is layered from
mandatory defaults, through one matching anchored rule, one explicit source
group, and finally an optional exact fixture override:

```cmake
verona_fixture_defaults(
  VBC_STAGE run
  LLVM_STAGE none)

verona_fixture_rule(
  MATCH "^(v|vir)/compile_only/"
  VBC_STAGE compile)

verona_fixture_group(
  SOURCES
    v/hello/hello.v
    vir/scalar_ops/scalar_ops.vir
  LLVM_STAGE run)

verona_fixture(
  SOURCE vir/dynamic_dispatch/dynamic_dispatch.vir
  LLVM_STAGE run
  LLVM_VALIDATOR llvm/cmake/validate_dynamic_dispatch_switch.cmake)
```

`VBC_STAGE` accepts `none`, `compile`, or `run`. `LLVM_STAGE` accepts `none`,
`emit-ir`, `assemble`, `codegen`, `link`, or `run`. A terminal stage includes
all earlier stages in that backend's graph. Explicit `none` makes unsupported
pipelines visible during review as backend coverage expands.

Every discovered canonical fixture receives effective metadata. Configuration
rejects missing defaults, unanchored, overlapping, duplicate, or unmatched
rules, duplicate group membership, noncanonical group sources, unknown or
duplicate exact overrides, invalid stages, and validators attached to disabled
LLVM pipelines. Auxiliary Verona source files whose basename differs from their
enclosing directory are not independent fixtures.

## Registration and Labels

Collections discover candidates by source extension, query the manifest, and
register nodes only through the selected backend stage. Metadata controls
registration; labels are derived descriptions used only for CTest selection
and reporting:

```text
compiler
frontend:vc
frontend:virc
backend:vbc
backend:llvm
runtime
runtime:vrt
```

A compiler node receives one frontend label from its source path and one
backend label from the collection registering it. `runtime:vrt` marks direct
VRT tests and the small number of compiler fixtures whose stated purpose is
VRT behavior. It is not applied to every native executable merely because the
program links VRT.

Examples:

```bash
ctest -L '^backend:vbc$'
ctest -L '^frontend:virc$' -L '^backend:llvm$'
ctest -L '^runtime$' -L '^runtime:vrt$'
ctest -L '^compiler$' -L '^runtime:vrt$'
```

## Fixture Naming and Coverage

Fixture names describe tested behavior, not a backend. Backend eligibility
belongs in the manifest rather than `llvm_*` or `vrt_*` prefixes. When two
fixtures would acquire the same semantic name, either merge their assertions
or choose narrower behavioral names; record that decision in a dedicated
commit.

Compiler fixtures that exercise generated code against VRT remain under
`vir/`. Direct runtime fixtures remain under `vrt/`. Coverage headers or an
adjacent README state the exercised boundary and non-goals.

## Adding or Changing a Fixture

1. Add the canonical source and golden directories.
2. Confirm defaults and structural rules provide the intended stages; use a
  source group when several explicit fixtures share metadata, and an exact
  override for one fixture's validator, labels, or other exceptional behavior.
3. Add a validator only when generic LLVM structural validation is insufficient.
4. Document non-trivial native or runtime coverage at the source or adjacent README.
5. Reconfigure, build/install, run focused frontend/backend selections, then run the full suite.
6. Run `ninja update-dump` twice and inspect the second run for unexpected changes.

The global `update-dump` target aggregates `compiler-update-dump` and
`runtime-update-dump`. Generated artifacts remain in the build tree; only
declared goldens belong in source control.
