# VBC Format

Verona Bytecode (VBC) is the binary contract between the VBC emitter and VBCI.
Neutral constants and enums are defined in `include/vbc/format.h` under
namespace `vbc`.

## Encoding

- Unsigned integers and enum values use ULEB128.
- Signed integers use zigzag SLEB128.
- Floating-point constants are bit-cast and encoded with zigzag SLEB128.
- Strings are length-prefixed byte sequences.
- Debug information, when present, is appended as a Zstandard-compressed
  stream.

The file starts with `MagicNumber`, then `CurrentVersion`. Readers must reject
files with an unsupported magic or version before interpreting later sections.

## Section Order

The current encoder writes these sections in order:

1. magic and version;
2. execution string table;
3. class and complex-primitive counts;
4. primitive method tables;
5. class field and method tables;
6. complex-primitive method tables;
7. FFI libraries and symbols;
8. function metadata;
9. complex type encodings;
10. memo initialization function IDs;
11. bytecode size and instruction stream;
12. optional compressed debug strings, sources, and debug operations.

Counts, IDs, offsets, and instruction operands use ULEB128 unless an opcode's
definition states otherwise.

## Type IDs

`vbc::PrimitiveType` values are both the primitive IDs at the start of the type
table and the encoded type operand for `Const` and `Convert`. Their declaration
order is part of the format.

Class IDs follow primitive IDs. Complex type IDs follow class IDs. Complex type
records begin with `vbc::TypeTag` and encode arrays, cowns, references, unions,
or tuples. `DynId` is the reserved all-bits-one dynamic type ID.

VBCI's `ValueType` is not a wire enum. It deliberately mirrors primitive IDs at
its beginning, then adds interpreter layout categories and live tags such as
register references, functions, errors, and invalid values.

## Instructions and Regions

Every instruction begins with a ULEB128 `vbc::Op`; operand order is documented
beside each enum entry in `include/vbc/format.h`. Region allocation instructions
encode `vbc::RegionType`.

Adding or reordering an opcode, primitive type, type tag, region type, or debug
opcode changes the wire contract and requires a version decision.

## Debug Information

Debug operations use `vbc::DIOp`. The low two bits identify the operation and
the remaining bits encode its value. The compressed stream also contains a
debug string table and embedded source contents.

Stripped VBC files end after the instruction stream and omit this section.

## Compatibility

`include/vbci.h` is a migration header that imports the neutral VBC names into
namespace `vbci`. New producers and format-aware tools include
`vbc/format.h` directly. Interpreter live state remains private under `vbci/`.