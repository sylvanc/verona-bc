# VIRC

VIRC validates and analyzes Verona Intermediate Representation (VIR), then
provides output-neutral `virc::Compilation` state to peer emitters.

## Components

| Target | Ownership |
| --- | --- |
| `virc_core` | Shared passes, IDs, type state, liveness, and compilation model |
| `virc_reader` | Textual `.vir` parser and reader-only passes |
| `virc_vbc` | VBC serialization |
| `virc_llvm` | Optional LLVM IR emission |
| `virc` | Standalone textual-VIR compiler |

The shared pass order is defined by `virc::pipeline()` in `compile.cc`. VC appends
that pipeline after source reification; the standalone tool prepends its reader
passes. Emitters consume `const Compilation&` and do not own shared analysis.

`virc` is the CMake executable target. It links `virc_reader`, `virc_vbc`, and,
when enabled, `virc_llvm`. The `build` in `virc build input.vir` is a Trieste
Driver subcommand, not a separate CMake target.

`virc_reader` contains the parser and normalization passes needed when a
pipeline starts from textual `.vir`. VC already produces VIR in memory, so it
links `virc_core` and its selected emitters directly without `virc_reader`.

The executable links the header-only `CLI11::CLI11` target because its option
handling uses CLI11 through the Trieste Driver. Trieste supplies that CMake
target as one of its dependencies.

## Compatibility

The `vbcc` executable and CMake aliases remain compatibility surfaces during
the naming migration. New code uses VIRC names.

The `vbcc` executable is built from the same entry point as `virc` and remains
installed as `dist/vbcc/vbcc` while scripts migrate to `dist/virc/virc`.

The build-tree aliases `libvbcc` and `vbc::vbcc` map to `virc_core` to ease
migration of old CMake target names. They do not reproduce the former aggregate
`libvbcc`, which also contained VBC serialization. Consumers that emit VBC
must link `virc_vbc` explicitly. New code should use only VIRC target names.

See [ADR 0001](../docs/architecture/0001-virc-and-output-backends.md), the
[VIR format](../docs/formats/vir.md), and the
[VC backend policy](../docs/architecture/vc-backends.md).
Implementation details are indexed in [VIRC Internals](docs/README.md).
