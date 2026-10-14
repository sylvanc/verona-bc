# Collector Fixture

## Purpose

This fixture verifies the native VRT collector's reentrant, two-phase
finalize-and-destroy queue. It drives the collector directly and through public
object, frame, and region lifetime operations; it does not exercise compiler
lowering or generated metadata.

## Scenarios

- Null header and region collection requests are harmless no-ops.
- Releasing a parent region can finalize an object, drop its child-region edge,
  and enqueue the now-unowned parent while the collector is already draining.
- An individually collected header is detached before finalization while its
  temporary owner guard keeps the former region out of destruction.
- A second collection request for an already-detached header is ignored
  without disturbing the owner guard or queuing duplicate storage release.
- A separately retained child region survives parent collection with its entry
  point and contents intact.
- Frame teardown uses the queued collector path and preserves independently
  retained child regions.
- Arena teardown uses the same queue while retaining bulk-reclamation
  semantics.

## Runtime Boundary

The fixture covers `collect(Header*)`, `collect(Region*)`, duplicate header
requests, temporary owner references used during destruction, and the
guarantee that queued finalization completes before corresponding storage is
released.

## Non-Goals

User-finalizer behavior is covered by the finalizer fixture. Immutable SCC
publication and reclamation are covered by the Freeze and SCC fixtures.
