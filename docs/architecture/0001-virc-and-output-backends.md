# ADR 0001: VIRC and Output Boundaries

- Status: Accepted
- Date: 2026-09-26

## Context

The component named VBCC combined four responsibilities: shared VIR analysis,
textual VIR input, compilation state, and VBC serialization. Its `Bytecode`
state type and `vbc::vbcc` target name made the VBC format appear to own passes
and state that are independent of any output representation.

The VBC wire contract was also declared in an interpreter-owned header. This
forced producers to depend on interpreter names and mixed serialized primitive
IDs with live interpreter value tags.

## Decision

Rename the shared compiler component to **VIRC**, the Verona Intermediate
Representation Compiler.

VIRC owns:

- shared VIR validation, analysis, and optimization passes;
- output-neutral `virc::Compilation` state;
- repository-internal compilation orchestration;
- the textual VIR reader used by the standalone command.

Output production is a dependency of VIRC consumers, not an ownership concern
of `virc_core`. The current VBC emitter consumes `const Compilation&` through
`virc::vbc::emit`.

The build is divided into:

- `virc_core`: shared passes, analysis, and compilation state;
- `virc_reader`: textual VIR parsing and reader-only normalization;
- `virc_vbc`: VBC serialization and compression;
- `virc`: the standalone textual VIR compiler.

Dependencies point toward the core:

```text
virc_reader -> virc_core
virc_vbc    -> virc_core
vc          -> virc_core + virc_vbc
```

Neutral contracts have independent owners:

- `include/vir.h` defines the VIR schema;
- `include/vbc/format.h` defines serialized VBC values and operations;
- `vbci/value_type.h` defines interpreter-only live value tags.

## Compatibility

During migration:

- `include/vbcc.h` forwards VIR names through namespace `vbcc`;
- `include/vbci.h` forwards VBC wire names through namespace `vbci`;
- `libvbcc` and `vbc::vbcc` alias `virc_core` in the build tree;
- the `vbcc` executable remains installed beside the primary `virc` command;
- `vbc::include` aliases the neutral `verona::include` target.

Compatibility names delegate to the new owners and do not create duplicate
implementations.

## Consequences

- VC can append the shared VIRC pipeline directly after reification.
- Textual VIR parsing is not linked into VC.
- VBC-specific dependencies and serialization stay outside `virc_core`.
- New output consumers can depend on `Compilation` without changing VIRC
  ownership.
- VBCI can consume the wire contract without exposing live runtime tags as
  serialized values.

## Alternatives Considered

### Keep the VBCC component name

Rejected because it assigns shared compiler ownership to one output format.

### Keep one aggregate compiler library

Rejected because consumers could not select shared analysis independently from
textual input and VBC emission.

### Stabilize a public embedding API immediately

Rejected. The API remains repository-internal until its compatibility and
installation requirements are understood.
