# VIRC Type System

**Current:** `virc/passes/typecheck.cc` validates the reified program and
records per-function local types and dynamic lookup information in
`Compilation`. `virc/analysis/ir_subtype.h` implements VIR subtyping over the
neutral schema, while `virc/analysis/sequent.h` provides the shared sequent
machinery.

Primitive identity is represented by `PrimitiveKind`. Compound type identity
is represented by `TypeInfo` and dense type IDs. This model is independent of
VBC wire tags and LLVM values; each emitter owns the conversion at its boundary.

VIRC checking is distinct from source inference. See
[VC type inference](../../vc/docs/compiler/type-inference.md) for the earlier
source-level pass and [VBC type encoding](../vbc/docs/type-encoding.md) for the
wire mapping.