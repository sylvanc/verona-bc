# SCC Fixture

## Purpose

This fixture verifies the lifetime semantics of already-published immutable
SCCs independently of Freeze discovery. It manually publishes components so
ARC forwarding, outgoing-edge release, and concurrent zero-transition behavior
can be tested without coupling failures to Tarjan traversal.

## Scenarios

- Non-representative members forward retain and release operations to the
  canonical representative.
- Two-object, self-cyclic, and three-object components are reclaimed as whole
  units when their representative ARC reaches zero, including finalization and
  outgoing-edge release from non-representative members.
- Collecting one immutable component decrements an outgoing edge to another
  component without prematurely collecting the target.
- Concurrent retainers through both representative and non-representative
  members increase one representative ARC, after which concurrent releasers
  produce exactly one transition to zero and one collection.
- Outgoing mutable edges are dropped exactly once during component collection.

## Runtime Boundary

The fixture covers `SccPtr` representative lookup, atomic ARC increments and
decrements, the collection-claim guard, member finalization, and two-phase
reclamation of every SCC member.

## Non-Goals

SCC discovery, mutable RC accounting, and immutable publication belong to the
Freeze fixture. Generated-code integration belongs to the native LLVM Freeze
fixture.
