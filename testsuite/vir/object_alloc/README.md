# LLVM object allocation fixture

This fixture verifies object allocation and region placement through both the
bytecode and native LLVM pipelines.

## Coverage

- Returning a non-empty object from a preserved frame-local allocation.
- Dragging that object while initializing a new RC-region root.
- Heap allocation in an existing RC region.
- Direct arena-region allocation and objects with `none` fields.
- Loading an empty class's immortal singleton data address.
- Calling a generated method on a value produced by a `singleton` statement.

## Native VRT coverage

VRT allocates frame-local, RC-region, heap-attached, and arena-region objects;
tracks the RC-region ownership relationships; and initializes the immortal
empty-class singleton consumed by generated code.

## Non-goals

The fixture does not exercise array allocation, dynamic dispatch, freezing,
arena teardown assertions, or object finalizers.