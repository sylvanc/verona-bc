# LLVM dynamic dispatch fallback fixture

This fixture verifies the runtime lookup path used when a dynamic receiver has
more classes than the compiler's bounded switch policy permits.

## Coverage

- Method lookup on a union of nine nominal classes.
- Runtime dispatch through the first and last class in the union.
- Ordinary dynamic calls whose implementations share one native signature.
- Emitted IR validation that requires `vrt_object_lookup` and rejects a class-ID
  accessor call or LLVM switch.

## Native VRT coverage

VRT initializes both immortal singleton receivers and searches their emitted
sorted method tables. The selected generated function descriptors are invoked
through the ordinary dynamic-call path.

## Non-goals

The fixture does not test the bounded switch path, dynamic tail calls, missing
methods, incompatible signatures, inheritance, or object fields.