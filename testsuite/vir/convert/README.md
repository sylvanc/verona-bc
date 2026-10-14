# LLVM conversion fixture

This fixture verifies VIR `convert` lowering through both the bytecode and
native LLVM pipelines. It accumulates mismatches into the process exit code.

## Coverage

- Signed extension, zero extension, and truncation across every VIR integer
  representation.
- Integer-to-float, float-to-integer, and `f32`/`f64` conversions.
- Conversions between numeric values, `bool`, and `none`.
- Raw-pointer conversions through integer, floating-point, Boolean, and
  `none` representations, including a non-null allocation check.

## Native VRT coverage

VRT starts the generated program and records the accumulated status through
`set_exit_code`. The allocation and release used by the pointer cases call the
host C library's `malloc` and `free`; they do not exercise VRT object or region
allocation.

## Non-goals

The fixture does not test out-of-range floating-point-to-integer behavior,
object-reference conversion, or conversion-driven dynamic dispatch.