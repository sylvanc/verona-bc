# Values and Layouts

`vrt::ValueType` classifies native representation and lifetime behavior as
none, scalar, raw pointer, object, array, reference, cown, dynamic, or aggregate.
It is not a source type enumeration and is not the VBC primitive wire enum.

Compiler-emitted `TypeInfo` supplies storage size and element metadata needed
by generic runtime operations. Class `Field` metadata records offset, size,
type ID, runtime value category, and the global VIR field ID used to construct
a field reference.

`vrt_reference` is a fixed-layout, multiword value. It contains a reference
kind, an owner, the target storage address, the content type ID, and a frame
storage epoch. Field and array references own one root reference to their
container. Register references borrow a generated-code variable slot and are
valid only while their logical frame is active and its native storage epoch is
unchanged.

`vrt_reference_from_register`, `vrt_reference_from_field`, and
`vrt_reference_from_array` construct references. The field and array forms
consume an already-owned container value. `vrt_reference_load` creates root
ownership for a managed result; `vrt_reference_exchange` consumes the incoming
root and returns the outgoing value with root ownership.

VBCI's boxed `Value` remains interpreter-private and does not cross this ABI.