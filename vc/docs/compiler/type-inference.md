# Type Inference

**Current:** `vc/passes/infer.cc` performs per-function bidirectional
refinement after ANF and before reification. It tracks local types by VIR
location and propagates constraints forward from definitions and backward from
uses.

The pass refines default integer and floating literals from call parameters,
explicit variable types, object fields, stores through typed references, and
declared return types. Call inference can fill empty type arguments through
direct type-parameter matching. Later constraints can also propagate through
prior static and dynamic calls.

Control-flow processing keeps per-label environments. Return-type inference
uses narrowed label exit environments before the global environment, preserving
type-test refinements. Cascade tracking grows monotonically to prevent
oscillation when a local receives constraints from multiple sources.

The language-level behavior is described in
[Type Inference](../18-type-inference.md). Subtyping support lives in
`vc/subtype.h`; output-neutral VIR checking belongs to VIRC and is described in
the [VIRC type system](../../../virc/docs/type-system.md).