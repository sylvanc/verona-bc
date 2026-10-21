# VC Pipeline

**Current:** VC uses Trieste rewriting passes to lower Verona source to
reified VIR, then appends the shared VIRC pipeline in memory. It does not invoke
the standalone `virc` command or serialize VIR between the two stages.

The frontend pass sequence is defined in `vc/lang.cc`:

```text
parse -> structure -> ident -> sugar -> functype -> dot -> application
      -> anf -> infer -> reify
```

`vc/main.cc` then appends `virc::pipeline()`, whose pass order is documented in
the [VIRC pipeline](../../../virc/docs/pipeline.md). `reify` produces a tree
accepted by `vir::wfIR` in `include/vir.h`.

For a pass-by-pass user-facing overview and dump commands, see
[Compiler Pipeline](../20-compiler-pipeline.md). The
[VIR format](../../../docs/formats/vir.md) owns the frontend/backend contract.