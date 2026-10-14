# Finalizer Fixture

## Purpose

This fixture verifies native VRT finalizer execution across individual object,
region, frame, arena, and immutable-SCC reclamation. It supplies finalizer
thunks directly so the test can isolate runtime ordering and cleanup behavior
from LLVM metadata generation.

## Scenarios

- An individually released frame-local object runs its finalizer while its
  managed child remains readable.
- A finalizer may allocate and recursively collect another object.
- Frame, RC-region, and arena teardown invoke finalizers before dropping fields
  or releasing storage.
- A runtime error raised by a finalizer is contained by its cleanup boundary;
  the object still drops its fields and the caller frame remains usable.
- Every member of a frozen cyclic SCC is finalized before any member's storage
  is destroyed, allowing each finalizer to inspect its sibling.

## Runtime Boundary

The fixture covers `FinalizerThunk`, `Object::finalize`, collector reentrancy,
`ThreadContext::run_cleanup`, field-drop ordering, and two-phase SCC
reclamation.

## Non-Goals

LLVM generation of class finalizer metadata and the C-to-tailcc thunk is
covered by the native VIR finalizer fixture. Freeze graph discovery and ARC
publication are covered by the Freeze fixture.
