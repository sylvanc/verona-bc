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