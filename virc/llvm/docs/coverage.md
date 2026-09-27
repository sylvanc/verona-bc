# LLVM Emitter Coverage

**Current:** LLVM tests are explicit allowlists in `vc-llvm.cmake` and
`virc-llvm.cmake`. They compile textual LLVM IR, assemble it, lower it to a
native object, link it with VRT, and compare native execution with goldens.

The allowlists cover only operations whose lowering and VRT support are both
available. They are not evidence that every VBC/VBCI fixture has native parity.
Unsupported lowering must remain a clear compiler error.

Coverage expands by complete behavior, with equivalent interpreted and native
tests where VBCI-to-VRT migration is involved. See the
[backend policy](../../../docs/architecture/vc-backends.md) and
[test pipelines](../../../testsuite/docs/pipelines.md).

**Current:** `llvm_reference` covers all six VIR reference statement tokens,
reference copy/move ownership, function arguments and returns, typed raise
transport, and static tailcall validation through both VBCI and native VRT.