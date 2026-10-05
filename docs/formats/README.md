# Interchange Formats

Format documentation is owned here rather than by a producer or consumer.

## VIR

Verona Intermediate Representation is the typed, reified input to VIRC. Its
neutral schema lives in `include/vir.h`.

VIR is produced directly by VC and may also be read from textual `.vir` files
by the standalone compiler. VIRC validates and transforms VIR before either
output backend runs.

See [VIR Format](vir.md).

## VBC

Verona Bytecode is the serialized format emitted by the VBC backend and
executed by VBCI. Its wire definitions live in `include/vbc/format.h`.

The format contract includes magic and version values, opcodes, encoded type
and region identifiers, and debug-information opcodes. VBCI live-value tags
and platform macros are not part of the format.

See [VBC Format](vbc.md).

See [ADR 0001](../architecture/0001-virc-and-output-backends.md) for ownership
and compatibility decisions.