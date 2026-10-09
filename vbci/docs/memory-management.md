# Memory Management

VBCI implements frame-local allocation, reference-counted and arena regions,
objects, arrays, references, cowns, and write barriers. Registers carry owning,
borrowed, moved, or immortal values through explicit wrapper types.

Region ownership is represented in private `Region` and `Header` state. Stack
locations identify active frame-local storage. Write barriers drag or reference
values according to source and destination regions before a store is published.

Source-level ownership semantics are documented in the
[Memory Model](../../vc/docs/19-memory-model.md).