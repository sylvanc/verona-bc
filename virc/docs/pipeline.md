# VIRC Pipeline

**Current:** `virc::pipeline()` in `virc/compile.cc` returns this shared pass
sequence:

```text
memo -> assign_ids -> validate_ids -> typecheck -> optimize -> liveness
```

`memo` rewrites reachable `once` functions and records initialization order.
ID assignment indexes definitions into `Compilation`, and validation checks the
resulting references. Type checking builds output-neutral type and lookup
state. Optimization rewrites VIR without selecting a backend. Liveness records
uses and inserts the ownership operations required by emitters.

VC supplies an in-memory `vir::wfIR` tree. The standalone tool first runs the
reader-only `statements` and `labels` passes over textual VIR. See the
[VIR format](../../docs/formats/vir.md) for the shared input contract.