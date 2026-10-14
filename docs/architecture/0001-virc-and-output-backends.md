# ADR 0001: VIRC and Output Backends

- Status: Accepted
- Date: 2026-09-23

## Context

The component named VBCC began as a VBC compiler, but now performs two distinct
jobs:

1. It validates, analyzes, optimizes, and indexes reified Verona IR.
2. It emits either VBC or LLVM IR.

The `Bytecode` class similarly combines output-neutral compilation state with
VBC encoding, while `include/vbcc.h` defines the VIR interchange schema rather
than a VBCC API. These names make VBC appear to own shared compiler state and
make LLVM look like a secondary mode of a bytecode implementation.

The VBC wire contract is also mixed with VBCI platform and live-value details
in `include/vbci.h`. This prevents producers and consumers from depending on a
neutral format definition.

## Decision

Rename the shared compiler component to **VIRC**, the Verona Intermediate
Representation Compiler.

VIRC owns:

- shared VIR validation and transformation passes;
- output-neutral analysis and indexed compilation state;
- the textual VIR reader used by the standalone tool;
- orchestration that produces a read-only `virc::Compilation`.

VIRC does not own an output format. VBC and LLVM are peer emitters that consume
the same read-only compilation:

```cpp
auto result = virc::compile(reified_vir);
virc::vbc::emit(result.compilation(), output);
virc::llvm::emit(result.compilation(), output);
```

The initial VIRC API remains repository-internal in `virc/compile.h`. It can be
promoted to a public installed API only after its ownership and stability needs
are understood.

Neutral contracts are owned by their formats:

- `include/vir.h` defines VIR tokens and well-formedness.
- `include/vbc/format.h` defines the VBC wire format and encoded values.
- VBCI platform and live interpreter state remain private to VBCI.

The migration creates these target components:

- **Target `virc_core`:** shared passes, analysis, and output-neutral
  compilation model;
- **Target `virc_reader`:** textual VIR input used by the standalone CLI;
- **Target `virc_vbc`:** VBC encoding and emission;
- **Target `virc_llvm`:** LLVM IR emission, built only when enabled;
- **Target `virc`:** the standalone CLI.

## Compatibility

**Migration:** retain a `vbcc` executable and CMake aliases for one transition
period. Compatibility names delegate to VIRC and do not define a second
implementation. New code and documentation use VIRC names.

VBC remains VC's default output regardless of whether LLVM support is compiled
in. Backend availability and default selection are separate decisions.

## Consequences

- VC can call VIRC directly without serializing and reparsing textual VIR.
- VBC and LLVM code no longer mutate or privately reinterpret shared compiler
  state.
- Format versioning can be documented independently of either implementation.
- VBCI can consume the VBC contract without exposing interpreter internals.
- The migration changes paths and target names, so temporary aliases and
  explicit compatibility tests are required.

## Alternatives Considered

### Keep the VBCC name

Rejected because it continues to imply that shared compilation belongs to the
VBC backend.

### Put LLVM emission in VC

Rejected because both the standalone VIR tool and VC need the same analyzed
compilation and emitter. Duplicating orchestration would create two backend
pipelines.

### Publish a stable VIRC API immediately

Rejected for the initial migration. The API should first prove that it is
output-neutral and useful to both VC and the standalone CLI.