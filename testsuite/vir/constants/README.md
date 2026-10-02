# LLVM special constants fixture

This fixture verifies the VIR `e`, `pi`, `inf`, and `nan` constants through
both the bytecode and native LLVM pipelines.

## Coverage

- Special constants as SSA definitions and assignments to a typed mutable
  destination.
- `e` and `pi` values converted to integers and checked at runtime.
- `f64` special constants returned from ordinary functions.
- Construction and drop lowering for infinity and NaN values.

## Native VRT coverage

VRT starts the generated program and records the accumulated result through
`set_exit_code`. Special floating-point constants and conversions lower to
native LLVM values and do not consume VRT object metadata.

## Non-goals

The fixture does not distinguish NaN payloads, check infinity signs, exercise
`f32` special constants, or perform floating-point arithmetic.