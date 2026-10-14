# Architecture

This directory contains cross-component decisions and policies. See
[Documentation Policy](../documentation-policy.md) for ownership rules.

## Decisions

- [ADR 0001: VIRC and Output Backends](0001-virc-and-output-backends.md)

Accepted ADRs are historical records. If a decision changes, add a new ADR that
supersedes the old one rather than rewriting the original context.

## Operational Policies

- [VC Backend Policy](vc-backends.md)
- [VBCI to VRT Migration Policy](vbci-vrt-migration.md)

Operational policies describe current migration constraints and should evolve
with the implementation.

## Component Flow

**Current:** the migration described by ADR 0001 implements this component
flow:

```text
Verona source -- VC --> VIR -- VIRC --> Compilation -- VBC emitter --> VBC -- VBCI
                                             |
                                             +-- LLVM emitter --> LLVM IR -- VRT
```

**Migration:** the implementation lives under `virc/` and uses namespace
`virc`. The `vbcc` executable, CMake aliases, and `include/vbcc.h` forwarding
header remain available for one transition period.