# VBCI

VBCI loads and executes Verona Bytecode. It owns interpreter frames, values,
regions, scheduling integration, FFI loading, and bytecode dispatch.

VBCI consumes neutral wire definitions from `include/vbc/format.h`. Platform
macros live in `platform.h`, while `value_type.h` and `value.h` define private
live interpreter representations. Those live tags are not part of VBC.

The executable target is `vbci` and the installed command is
`build/dist/vbci/vbci`. See the [VBC format](../docs/formats/vbc.md) and the
[VBCI to VRT migration policy](../docs/architecture/vbci-vrt-migration.md).

Implementation details are indexed in [VBCI Internals](docs/README.md).
