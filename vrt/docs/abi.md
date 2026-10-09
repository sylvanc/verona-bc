# VRT ABI

**Current:** public C and C++ declarations live under `include/vrt/` and use
`VRT_EXPORT` for externally visible symbols. Generated code links against
`libvrt` and calls these functions with C calling convention.

The ABI covers runtime and program initialization, invocation boundaries,
function and class metadata, threads and frames, values, references, objects,
arrays, regions, and errors. Implementation-only C++ types under `vrt/` are
not public ABI.

`include/vrt/reference.h` defines the fixed `vrt_reference` representation and
construction, retain/release, escape, tailcall validation, load, and exchange
entry points. Functions that create a reference or return a loaded/exchanged
value use explicit output storage, avoiding target-specific C aggregate-return
conventions.

Language raises use a compiler-emitted type ID plus a pointer to encoded native
storage. VRT copies that representation into continuation-owned storage before
unwinding frames. The target continuation consumes it into caller-provided
storage. This supports scalars, pointers, and multiword references through one
transport.

Changes to exported structure layout, enum values, or signatures require a
coordinated LLVM emitter and runtime update.