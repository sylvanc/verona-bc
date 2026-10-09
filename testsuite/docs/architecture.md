# Testsuite Architecture

## Ownership Layers

The compiler testsuite separates four concepts:

1. The **compiler suite** owns the public CTest namespace, automatic label, and update target.
2. A **collection** owns one frontend/output pipeline and registers its node graph.
3. A **fixture** is one canonical `.v` or `.vir` source.
4. A **node** is one executable stage with exact goldens, artifacts, and dependencies.

`testsuite/CMakeLists.txt` assigns current collection ownership explicitly:

```cmake
testsuite(
  compiler
  COLLECTIONS
    vc-vbc.cmake
    virc-vbc.cmake)
```

Explicit ownership allows later output collections to join without making every
adjacent CMake helper a collection.

## Fixture Metadata

`cmake/compiler_fixtures.cmake` is the authoritative feasibility manifest for
canonical sources under `v/` and `vir/`. Effective metadata is layered from a
mandatory default, through non-overlapping anchored rules, to optional exact
overrides:

```cmake
verona_fixture_defaults(
  VBC_STAGE run)

verona_fixture_rule(
  MATCH "^(v|vir)/compile_only/"
  VBC_STAGE compile)

verona_fixture(
  SOURCE vir/simp1/simp1.vir
  VBC_STAGE none)
```

`VBC_STAGE` accepts `none`, `compile`, or `run`. A terminal stage includes all
earlier stages in the graph. Explicit `none` keeps unsupported fixtures visible
during review.

Every discovered canonical fixture receives effective metadata. Configuration
rejects missing defaults, unanchored or overlapping rules, unknown exact
override paths, duplicate rules/overrides, and invalid stages. Auxiliary Verona
source files whose basename differs from their enclosing directory are not
independent fixtures.

## Registration and Labels

Collections discover candidates by source extension, query the manifest, and
register nodes only through `VBC_STAGE`. Metadata controls registration; labels
are derived descriptions used only for CTest selection and reporting:

```text
compiler
frontend:vc
frontend:virc
backend:vbc
```

Examples:

```bash
ctest -L '^backend:vbc$'
ctest -L '^frontend:vc$'
ctest -L '^frontend:virc$' -L '^backend:vbc$'
```

## Adding or Changing a Fixture

1. Add the canonical source and golden directories.
2. Confirm defaults and structural rules provide the intended VBC stage; add an
  exact override only when the fixture differs.
3. Reconfigure, build/install, run the focused frontend selection, then run the full suite.
4. Run `ninja update-dump` twice and inspect the second run for unexpected changes.

The global `update-dump` target includes `compiler-update-dump`. Generated
artifacts remain in the build tree; only declared goldens belong in source
control.
