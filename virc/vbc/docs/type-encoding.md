# VBC Type Encoding

**Current:** `type_encoding.cc` maps output-neutral `PrimitiveKind` values and
compound `TypeInfo` records to VBC wire identities. Compile-time assertions pin
primitive ordering to `vbc::PrimitiveType` so an internal reorder cannot
silently alter serialized meaning.

Primitive IDs occupy the initial range. Class IDs follow, then compound array,
cown, reference, union, and tuple records. `encode_region()` maps VIR region
tokens to `vbc::RegionType`.

VBCI converts encoded identities to private interpreter representation at the
load boundary. The wire definitions and compatibility rules are authoritative
in the [VBC format](../../../docs/formats/vbc.md).