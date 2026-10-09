# Bytecode Loading

`Program::load()` reads the VBC file and decodes execution strings, class and
function metadata, complex types, memo initialization IDs, and the instruction
stream. Numeric operands use the ULEB and zigzag readers in `program.h`.

The loader validates format identity before treating later bytes as metadata.
It then fixes up method and function references and derives private interpreter
layouts from encoded type IDs. Optional debug data is decompressed lazily.

Section order and versioning are authoritative in the
[VBC format](../../docs/formats/vbc.md).