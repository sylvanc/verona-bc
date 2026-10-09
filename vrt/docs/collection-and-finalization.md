# Collection and Finalization

## Collector Contract

VRT exposes separate internal collection entry points for headers and regions.
The distinction is semantic:

- collecting a mutable header removes it from its region before queueing it;
- collecting an immutable SCC claims its canonical representative once and
  queues every member header;
- collecting a region claims the region's finalizing state and queues its
  remaining contents as one unit.

Work items are typed header or region records. A header item may carry an
`owner_guard`: a temporary stack reference to the mutable region from which
the header was detached. The collector releases that guard only after the
header's storage is destroyed. This prevents finalization of the former owner
region from overlapping the detached header's finalizer or storage
destruction.

Queueing an already-detached header or an already-claimed region or SCC is a
no-op. SCC callers may hold any member, but collection always claims and walks
the canonical representative.

## Finalize and Release Phases

The collector drains to a fixed point in two phases:

1. dequeue headers and regions in FIFO order, run header finalizers and region
   content finalization, and queue any work discovered by those finalizers;
2. destroy finalized header storage before bulk-releasing finalized regions,
   then release header owner guards.

Releasing storage or an owner guard may enqueue more collection. The collector
therefore repeats both phases until no work remains. Nested `collect` calls add
work to the active collector state instead of starting a recursive drain.

Object metadata identifies fields that must be traced or released and provides
an optional generated finalizer thunk. Finalization runs only after the object
or region has entered its finalizing state.

Fixtures document the exact finalization ordering they assert; see
[VRT coverage](coverage.md).