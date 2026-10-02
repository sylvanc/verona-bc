# Stack Storage API Coverage

This fixture exercises VRT-managed logical-frame storage through the public
object and array stack-allocation entry points. It verifies over-aligned object
data, oversized dynamic arrays, zero initialization, nested-frame cleanup,
generated finalizer thunks, tailcall frame reuse, and storage reuse.

The fixture inspects private headers only to confirm that stack allocations use
the active frame's `Location` and do not belong to a region. Stack ownership
barriers, escape rejection, and generated LLVM calls are covered separately.