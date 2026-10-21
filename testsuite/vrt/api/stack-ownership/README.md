# Stack Ownership API Coverage

This fixture verifies the lifetime boundary between VRT logical-frame stack
storage and longer-lived storage. Same-frame and older-frame stack references
are accepted when the owner survives, including values returned or raised from
a child frame.

The public APIs reject current-frame object, array, and field-reference escapes
and tailcall arguments. They also reject stack values stored into a region and
younger stack values stored into older stack objects. Generated LLVM lowering
is covered by compiler fixtures rather than this direct runtime fixture.