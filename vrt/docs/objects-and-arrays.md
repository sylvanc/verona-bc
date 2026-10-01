# Objects and Arrays

Object descriptors contain class identity, field layout, method dispatch
entries, singleton storage, and optional finalizer metadata. Object allocation
entry points create logical-frame stack values, create frame-local values,
allocate into an existing region, or create a new RC or arena region.

Arrays expose equivalent allocation, retain, release, pin, unpin, freeze, and
escape operations plus checked copy, fill, and lexicographic comparison over
encoded elements. Pinning and mutable region merge are documented in
[Regions and Ownership](regions-and-ownership.md).

Stack objects and arrays use the active frame's stack `Location`, but their
headers and zero-initialized data live in VRT-managed chunk storage rather than
native `alloca` storage. Retain and release are no-ops for these headers.
Frame cleanup still runs generated object finalizers and drops managed object
fields or array elements before reusing the storage.

Stack storage may reference values owned by the same frame or an ancestor
frame. The write barrier rejects stack values stored into regions and values
from a younger frame stored into older stack storage. Escape and tailcall
validation reject object, array, and reference values whose owning frame would
be destroyed. The language-level lifetime rules are authoritative in
[Memory Model](../../vc/docs/19-memory-model.md).

Generated code passes object data addresses and array element-storage pointers;
runtime headers remain private.