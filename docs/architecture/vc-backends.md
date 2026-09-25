# VC Backend Policy

This policy defines how VC selects and tests output backends during the VIRC
migration.

## Current State

VC hands reified VIR directly to VIRC and can emit VBC or LLVM IR when the LLVM
backend is enabled. VBC remains the default. The standalone VIRC tool accepts
textual VIR and exposes the same output choices. Disabling
`VERONA_ENABLE_LLVM_BACKEND` avoids the LLVM dependency and registers no LLVM
tests.

## Invariants

1. VBC remains the default output of `vc build`.
2. Building with `VERONA_ENABLE_LLVM_BACKEND=ON` makes LLVM IR available from
   VC; it does not change the default.
3. Building with `VERONA_ENABLE_LLVM_BACKEND=OFF` does not require LLVM and does
   not register LLVM tests.
4. VC hands reified VIR directly to VIRC shared compilation. It does not invoke
   the standalone VIRC executable or round-trip through textual VIR.
5. Backend failures are reported explicitly. VC does not silently fall back to
   VBC after an LLVM request fails.

## CLI Contract

**Current:** VC accepts an explicit output selection:

```text
vc build <source-directory> --emit vbc
vc build <source-directory> --emit llvm-ir
```

Omitting `--emit` is equivalent to `--emit vbc`. Requesting `llvm-ir` from a
build without LLVM support is a command-line error.

Backend-specific output names and link steps remain explicit. LLVM IR emission
does not imply native linking.

## Build Graph

**Current:** VC always links `virc_core` and `virc_vbc`. It links `virc_llvm`
only when `VERONA_ENABLE_LLVM_BACKEND` is enabled.

The standalone VIRC CLI follows the same rule. `virc_core` must not depend on
LLVM headers or libraries.

## Test Policy

**Current:** the existing VC collection is the compatibility pipeline:

```text
.v -> VC -> VBC -> VBCI
```

**Current:** source-to-LLVM coverage is a separate, explicitly selected
collection:

```text
.v -> VC -> LLVM IR -> llvm-as -> llc -> native link with VRT -> run
```

Only allowlisted fixtures whose required language and runtime behavior is
supported by the LLVM/VRT path belong in that collection. The initial fixture
is `v/hello`. Enabling LLVM does not send every VC fixture through the
incomplete native pipeline. With LLVM disabled, the collection registers no
tests.

**Current:** textual VIR backend tests are independent. The VBC collection
covers `.vir` fixtures broadly; the LLVM collection explicitly selects its
supported `.vir` fixtures:

```text
.vir -> VIRC -> VBC -> VBCI
.vir -> VIRC -> LLVM IR -> native link with VRT -> run
```

Fixtures named `llvm_*` run through both paths. In particular,
`llvm_reference` verifies register, field, and array references plus load,
store, return, raise, and tailcall integration against VBCI and native VRT.
Fixtures named `vrt_*` remain native-only when they exercise behavior that
does not yet have an interpreted adapter.

## Updating This Policy

Update this document when default selection, backend availability, linking, or
test registration changes. Record a new ADR if VBC ceases to be the default or
if backend ownership changes.