# LLVM bulk array operations fixture

This fixture verifies bulk array operations through both the bytecode and
native LLVM pipelines, with mismatches reported through the process exit code.

## Coverage

- Filling complete and partial ranges of primitive arrays.
- Copying between distinct arrays and copying overlapping ranges within one
  array.
- Three-way range comparison for equal and different primitive arrays.
- Zero-length fill, copy, and comparison at the end of an array.
- Filling and copying arrays whose elements are object references.

## Native VRT coverage

The generated program uses VRT array allocation and bulk fill, copy, and
comparison operations. The object-array cases also exercise ownership-aware
bulk handling for managed references, while the overlap case requires
memmove-style copy semantics.

## Non-goals

The fixture does not test bounds failures, partially overlapping distinct
allocations, nested arrays, or ordering assertions beyond equality versus
inequality.