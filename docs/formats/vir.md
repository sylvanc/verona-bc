# VIR Format

Verona Intermediate Representation (VIR) is the typed, reified contract between
VC and VIRC. It is represented as a Trieste AST whose token definitions and
well-formedness schema are in `include/vir.h` under namespace `vir`.

## Producers and Consumers

VC produces VIR directly after reification. The standalone VIRC reader also
constructs the same tree from textual `.vir` fixtures. Shared VIRC passes
validate, analyze, optimize, and index the tree before an output backend runs.

VBC and LLVM emitters consume the validated form. VBCI and VRT do not consume
VIR directly.

## Root Shape

`vir::wfIR` defines the authoritative tree shape. A top-level VIR tree contains
definitions such as:

- primitive, class, type-alias, function, symbol, and library definitions;
- optional memo initialization metadata;
- function parameters, variables, labels, statement bodies, and terminators;
- explicit type, ownership, region, call, FFI, and source-location nodes.

Identifiers are symbolic tokens before VIRC's ID-assignment pass. The indexed
compilation model is derived state and is not part of VIR.

## Textual Form

Textual `.vir` is a test and tool input syntax parsed by the standalone reader.
It is not an additional semantic IR. The parser must produce a tree accepted by
`vir::wfIR`, and VC may bypass it by handing reified nodes directly to VIRC.

## Compatibility

VIR currently has no serialized version field or external stability guarantee.
Changes to `vir::wfIR` must update all producers, shared passes, emitters, the
textual reader, fixtures, and this document in one change.

`include/vbcc.h` is a migration header that imports `vir` names into namespace
`vbcc`. New code includes `vir.h` directly.