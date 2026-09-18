# LLVM object allocation fixture

This fixture verifies object allocation and region placement through both the
bytecode and native LLVM pipelines.

## Coverage

- Returning a non-empty object from a preserved frame-local allocation.
- Dragging that object while initializing a new RC-region root.
- Heap allocation in an existing RC region.
- Direct arena-region allocation and objects with `none` fields.
- Empty-class singleton representation for both frame-local and heap syntax.
- Calling a generated method on empty-class values from both allocation paths.

## Native VRT coverage

VRT allocates frame-local, RC-region, heap-attached, and arena-region objects;
tracks the RC-region ownership relationships; and supplies the immortal
empty-class singleton used by both `new` and `heap` expressions.

## Non-goals

The fixture does not exercise array allocation, dynamic dispatch, freezing,
arena teardown assertions, or object finalizers.