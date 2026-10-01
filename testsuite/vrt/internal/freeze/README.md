# Freeze Fixture

## Purpose

This fixture verifies native VRT graph freezing: read-only preflight, Tarjan
SCC discovery, RC-to-ARC transfer, immutable publication, and ownership
transitions across regions. It uses public object and array Freeze entry points
where an exported API exists and private state only for precise invariant
checks.

## Scenarios

- Immortal and already-immutable values treat Freeze as a no-op.
- Acyclic graphs publish separate immutable components and cyclic graphs publish
  one canonical SCC representative; re-freezing through a non-representative
  `SccPtr` is a no-op.
- Incoming mutable edges, outgoing immutable edges, and edges to earlier SCCs
  contribute the correct ARC and stack-reference adjustments, including when
  the outgoing edge belongs to a non-representative SCC member.
- Partial-region and interior-object Freeze preserve mutable survivors and do
  not consume an ownership edge unless the region entry point is frozen.
- Owned subregions transition deepest-first, including sibling regions and
  nested ownership chains.
- Frame-local graphs freeze in place, leave their frame region empty, and
  delegate reachable heap subregions to the regular region Freeze path.
- Primitive arrays and arrays containing managed objects follow the same deep
  Freeze and reclamation rules as objects.
- Stack and arena roots are rejected through the public API, and reachable
  arenas fail preflight without partially mutating locations, reference counts,
  ownership, or region membership.

## Runtime Boundary

The fixture covers `freeze`, `freeze_local`, public object and array Freeze
entry points, SCC representative publication, ARC initialization, region
parent clearing, and `BadFreeze` recovery.

## Non-Goals

LLVM Freeze ownership lowering is covered by the generated `freeze_ownership`
fixture. User
finalizer ordering is covered by the finalizer fixture, and concurrent lifetime
operations on an already-published SCC are covered by the SCC fixture.
