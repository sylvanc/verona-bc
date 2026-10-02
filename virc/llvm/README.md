# LLVM Emitter

The optional LLVM emitter lowers a read-only `virc::Compilation` to textual
LLVM IR. Its build target is `virc_llvm`, enabled by
`VERONA_ENABLE_LLVM_BACKEND`; its public entry point is
`virc::llvm_backend::emit()`.

This component depends on LLVM and targets the public VRT C ABI. It does not
implement runtime behavior. `virc_core` and `virc_vbc` remain independent of
LLVM headers and libraries.

Supported source fixtures are explicitly allowlisted in the LLVM test
collections because VRT does not yet implement every VBCI subsystem. See the
[VC backend policy](../../docs/architecture/vc-backends.md) and
[VBCI to VRT migration policy](../../docs/architecture/vbci-vrt-migration.md).

## Documentation

- [Lowering](docs/lowering.md)
- [Type representation](docs/type-representation.md)
- [Metadata](docs/metadata.md)
- [Calls and control flow](docs/calls-and-control.md)
- [Allocation and ownership](docs/allocation-and-ownership.md)
- [VRT interface](docs/vrt-interface.md)
- [Coverage](docs/coverage.md)
