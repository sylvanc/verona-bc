# Register Values

`Value` is VBCI's private boxed representation. It stores primitive scalars,
pointers, objects, arrays, cowns, references, functions, and error state in a
tagged two-word value. `Register`, `ValueTransfer`, `ValueBorrow`, and
`ValueImmortal` express ownership at interpreter boundaries.

`vbci::ValueType` deliberately matches `vbc::PrimitiveType` only for the
primitive prefix. Its remaining entries classify interpreter layouts and live
register states and are never encoded as VBC primitive IDs.

Conversion from wire type identity occurs in `Program::layout_type_id()`.