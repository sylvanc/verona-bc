# VBC Emitter

The VBC emitter serializes a read-only `virc::Compilation` into the VBC binary
format. Its build target is `virc_vbc`; the public entry point is
`virc::vbc::emit()`.

The current implementation is intentionally concentrated in `emitter.cc`.
That file owns section assembly, instruction encoding, type serialization,
debug information, compression, and output writing. Internal decomposition is
a separate refactoring and does not change the `virc_vbc` boundary.

Wire constants, opcodes, and encoded enums are owned by
`include/vbc/format.h`. Shared VIRC state uses neutral type structures and pins
numeric compatibility to the wire contract with compile-time assertions at the
emitter boundary.

See the authoritative [VBC format](../../docs/formats/vbc.md). VBCI is the
consumer; interpreter live-value tags are not part of this emitter contract.
