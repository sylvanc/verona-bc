# LLVM Type Representation

`LoweredType` records three related choices: an LLVM SSA type, an addressable
storage type, and a public `vrt::ValueType` runtime category. `LoweredValue`
adds the emitted LLVM value and an optional callable signature.

Value-storage helpers allocate addressable slots in the function entry block,
materialize SSA values into those slots, and load values back into SSA form.
They are shared lowering machinery used by mutable locals and VRT ABI calls,
not part of raise handling itself.

Primitive lowerers live under `types/`. Nominal classes use generated structure
layouts for their fields while object values use runtime-managed pointers.
Arrays carry an element runtime type ID. Dynamic and aggregate forms have local
lowering rules and are not VBCI boxed values.

`ref[T]` lowers to the named `%vrt.reference` aggregate matching the public
`vrt_reference` layout. Generated Verona functions pass and return that
aggregate by value. Calls across the C ABI materialize it in temporary storage
and pass a pointer, which avoids platform-specific aggregate-return rules.

The public runtime categories are defined in `include/vrt/value.h`. Private
VBCI register tags do not participate in native representation.