# Reification

**Current:** `vc/passes/reify.cc` monomorphizes the reachable source program and
produces `vir::wfIR`. Reification starts at `main`, queues referenced classes,
functions, and type aliases, and deduplicates specializations by their applied
type substitutions.

The pass assigns symbolic `ClassId` and `FunctionId` nodes, lowers ANF forms to
VIR statements, and removes source-only structure. Shapes become dynamic types
rather than emitted class definitions. Primitive and wrapper types are queued
when their runtime representation is required.

Reification does not assign VBC numeric IDs and does not select an output
backend. Those responsibilities belong to the shared
[VIRC pipeline](../../../virc/docs/pipeline.md) and peer emitters. The output
tree contract is the [VIR format](../../../docs/formats/vir.md).