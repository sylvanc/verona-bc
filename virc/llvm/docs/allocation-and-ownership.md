# Allocation and Ownership

Object and array allocation lowerers call VRT entry points for new, heap, or
region allocation. Generated class metadata supplies object layout and dynamic
method information; array allocation supplies element representation metadata.

**Current:** retain and release are implemented for VRT object, array, and
reference representations. A field or array reference retains its owner;
register-reference retain and release are validated no-ops. Scalars, raw
pointers, and `none` require no lifetime calls.

**Target:** retain and release lowering for cowns, dynamic values, and
non-reference aggregate values is not implemented. Encountering those paths
reports an emission error. Runtime ownership semantics are authoritative in
[VRT Regions and Ownership](../../../vrt/docs/regions-and-ownership.md).