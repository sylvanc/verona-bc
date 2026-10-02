# VBC Encoding

`encoder.h` and `encoder.cc` provide the backend's byte-buffer primitives.
Unsigned values use ULEB128. Signed integral values use zigzag encoding followed
by ULEB128; floating values are bit-cast before the same signed encoding.
Strings are emitted as a length followed by their bytes.

`emitter.cc` owns section order and file output. `string_table.cc` serializes
the execution and debug name tables, while `instruction_encoder.cc` owns the
instruction stream.

The authoritative section order, magic, version, and opcode contract are in
the [VBC format](../../../docs/formats/vbc.md) and `include/vbc/format.h`.