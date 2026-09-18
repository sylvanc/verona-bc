# LLVM array allocation fixture

This fixture verifies array allocation and ownership paths through both the
bytecode and native LLVM pipelines.

## Coverage

- Dynamic and constant-size frame-local array allocation.
- Returning arrays from preserved call frames and raising an array to an
  enclosing continuation.
- Dynamic and constant-size heap arrays attached to an RC-region locator.
- Direct allocation in RC and arena regions.
- Filling and dropping arrays created by each allocation path.

## Native VRT coverage

The generated program asks VRT to allocate array storage in frame-local, heap,
RC-region, and arena-region contexts. VRT also handles the ownership changes
needed when arrays leave their allocating call frame or are attached to an RC
region.

## Non-goals

The fixture does not inspect array elements, compare or copy ranges, test
bounds failures, or allocate arrays of managed object references.