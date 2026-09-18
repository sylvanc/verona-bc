# VRT array runtime fixture

This fixture exercises the public array allocation, ownership, and bulk-operation
ABI together with the private representation needed to validate its effects.

## Coverage

- Frame-local `new`, existing-region `heap`, and fresh-region allocation.
- Compiler-emitted array type metadata and element-layout lookup.
- Contiguous zero-initialized element storage and payload/header conversion.
- Register retain/release and collection through generic `Header` dispatch.
- Managed object and nested-array element tracing and finalization.
- Graph dragging when an array is returned or raised across frame teardown.
- Primitive and managed-element copy/fill plus primitive comparison.

## Non-goals

LLVM array lowering, freezing, and immutable-array ownership are covered by
separate fixtures.
