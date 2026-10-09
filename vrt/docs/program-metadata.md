# Program Metadata

The LLVM emitter defines an external `verona_program` value described by
`vrt::Program`. It points to immutable type metadata and singleton metadata.

Each `TypeInfo` contains the generated type ID, a `ValueType` lifetime category,
native storage size, and an array element type ID where applicable. Each
`Singleton` pairs compiler-emitted storage with its class descriptor.

VRT validates and indexes this data during program initialization. The producer
side is described in [LLVM Metadata](../../virc/llvm/docs/metadata.md).