# Regions and Ownership

VRT supports reference-counted and arena regions selected by `vrt_region_type`.
Frame-local allocation belongs to the current logical frame; heap allocation
uses an object in the destination region as its locator; region allocation
creates a new entry point.

The internal ownership module distinguishes root references from field
references. A root keeps both an allocation and any non-frame-local region
reachable from outside its object graph alive. A field reference retains the
allocation while region parenting or stack-reference accounting keeps its
region alive.

Object, array, and reference retain/release ABI operations manage root
obligations. A field or array `vrt_reference` owns its container root, not the
value currently stored in the target. A register reference instead borrows a
frame variable slot.

## Pinning

`vrt_object_pin()` and `vrt_array_pin()` add exactly one root obligation for
external code. The caller must balance each successful pin with the matching
unpin operation.

Pinning a frame-local value first relocates its reachable graph into a fresh RC
region while preserving the value's existing root, then adds the pin root.
Stack allocations cannot be relocated and raise `VRT_ERROR_BAD_STACK_ESCAPE`.
Immutable values use their canonical SCC representative's atomic reference
count. Immortal values need no additional lifetime state.

Unpin consumes one root obligation; it does not track a separate per-value pin
counter. Cown pinning is not part of the current VRT ABI because VRT does not
yet own a cown subsystem.

## Mutable Region Merge

`vrt_region_merge()` accepts the runtime value type and data address for two
object or array operands. It gives both values one mutable ownership domain
without changing their root obligations:

- values already in one region, two frame-local values, and values without two
  mutable regions are no-ops;
- combining a stack allocation with a mutable region raises
  `VRT_ERROR_BAD_STACK_ESCAPE`;
- a frame-local graph is relocated into the other operand's RC region;
- two distinct RC regions are unified, preferring an already-owned region as
  the destination;
- two owned regions, ancestor/descendant regions, and attempted unification
  involving an arena region raise `VRT_ERROR_BAD_MERGE`.

Heap-region merge is a preflighted transaction. Before changing either graph,
VRT validates region state, ancestry, count capacity, every source header, and
every directly owned child region. Commit then guards both regions, moves the
source headers, reparents direct child regions, transfers the remaining
external stack-reference contribution, and destroys the empty source. A
rejected preflight leaves header locations, membership, parent metadata, and
reference counts unchanged.

Escape operations relocate current-frame-local values when they must outlive
the frame. Reference escape relocates a field or array owner; a register
reference may escape only when its defining frame survives the return or
raise. Tailcalls reject references into the current native activation.

Write barriers maintain ownership when values cross region boundaries.
Reference exchange consumes one incoming root into the target and transfers
the outgoing field obligation into a root result. Dragging, parenting, and
stack-reference changes complete before an operation can expose invalid
storage.

Language semantics remain authoritative in the
[Memory Model](../../vc/docs/19-memory-model.md).