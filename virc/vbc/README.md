# VBC Emitter

The VBC emitter serializes a read-only `virc::Compilation` into the VBC binary
format. Its build target is `virc_vbc`; the public entry point is
`virc::vbc::emit()`.

The backend is split by responsibility:

- `emitter.cc` assembles VBC sections and writes the file.
- `instruction_encoder.cc` encodes VIR statements and terminators.
- `debug_info.cc` tracks source locations and compresses debug data.
- `type_encoding.cc` serializes type and region metadata.
- `string_table.cc` serializes execution and debug string tables.
- `encoder.cc` provides byte-buffer and integer encoding primitives.

Wire constants, opcodes, and encoded enums are owned by
`include/vbc/format.h`. Shared VIRC state uses neutral type structures and pins
numeric compatibility to the wire contract with compile-time assertions at the
emitter boundary.

See the authoritative [VBC format](../../docs/formats/vbc.md). VBCI is the
consumer; interpreter live-value tags are not part of this emitter contract.

## Documentation

- [Lowering](docs/lowering.md)
- [Binary encoding](docs/encoding.md)
- [Type encoding](docs/type-encoding.md)
- [Debug information](docs/debug-information.md)
- [Coverage](docs/coverage.md)
