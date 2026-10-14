# Allocation and Ownership

Object and array allocation lowerers call VRT entry points for stack, new,
heap, or region allocation. `Stack`, `StackArray`, and `StackArrayConst` use
VRT-managed logical-frame storage rather than LLVM `alloca`; dynamic and
constant arrays share the same runtime allocation contract. Generated class
metadata supplies object layout and dynamic method information; array
allocation supplies element representation metadata.

**Current:** retain and release are implemented for VRT object, array, and
reference representations. A field or array reference retains its owner;
register-reference retain and release are validated no-ops. Scalars, raw
pointers, and `none` require no lifetime calls.

Before frame reuse, tailcall lowering asks VRT to validate object, array, and
reference arguments. VRT rejects values backed by the outgoing frame's stack
storage while accepting values owned by surviving ancestor frames. The
allocator and cleanup order are described in
[VRT Threads and Frames](../../../vrt/docs/threads-and-frames.md).

**Target:** retain and release lowering for cowns, dynamic values, and
non-reference aggregate values is not implemented. Encountering those paths
reports an emission error. Runtime ownership semantics are authoritative in
[VRT Regions and Ownership](../../../vrt/docs/regions-and-ownership.md).