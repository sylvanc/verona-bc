# LLVM Metadata

The emitter creates function and class descriptors used by dynamic lookup and
runtime calls. `define_program_metadata()` also emits the external
`verona_program` descriptor containing runtime type records and singleton
initialization entries.

Runtime type records identify a VRT value category, storage size, and array
element type where applicable. IDs are derived from the output-neutral VIRC
model so metadata and lowered operations refer to the same definitions.

The consuming ABI is documented in
[VRT Program Metadata](../../../vrt/docs/program-metadata.md).