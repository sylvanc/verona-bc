# VRT array allocation and ownership fixture

This fixture exercises the public array allocation and ownership ABI together
with the private representation needed to validate its effects.

## Coverage

- Frame-local `new`, existing-region `heap`, and fresh-region allocation.
- Compiler-emitted array type metadata and element-layout lookup.
- Contiguous zero-initialized element storage and payload/header conversion.
- Register retain/release and collection through generic `Header` dispatch.
- Managed object and nested-array element tracing and finalization.
- Graph dragging when an array is returned or raised across frame teardown.

## Non-goals

Bulk copy, fill, comparison, LLVM array lowering, freezing, and immutable-array
ownership are covered by later fixtures.
