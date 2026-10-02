# Freeze and SCCs

## Freeze Phases

Native Freeze has explicit preflight, discovery, publication, accounting, and
region-transition phases:

1. A read-only preflight traces the complete reachable graph. Unsupported
   arena, stack, pending, finalizing, or destroying state rejects the operation
   before any object is mutated.
2. Discovery removes reachable headers from one RC region at a time and marks
   them `Pending`. Tarjan-style traversal and union-find collapse cycles while
   transferring reference counts for tree and back edges.
3. Publication assigns ARC counts to canonical SCC representatives, changes
   them to `Immutable`, and changes every other SCC member to `SccPtr`.
4. Accounting subtracts edges between newly frozen components, edges from
   surviving mutable objects, and any parent ownership edge from the region's
   accumulated external reference count.
5. Recorded ownership and stack-reference transitions are applied in reverse
   discovery order, so nested regions transition deepest first.

`Pending` is private, transient Freeze state. Published SCC collection does
not return objects to `Pending`; it atomically claims the immutable
representative and forwards member operations through `SccPtr`.

## Region Boundaries

Freeze records a parent transition only when the newly frozen set contains an
owned region's entry point. Freezing an interior object preserves the entry
point's ownership edge.

Frame-local objects are published in place and removed from their logical
frame region. Reachable heap subregions are then delegated to the regular
region Freeze path. Primitive arrays and arrays of managed values use the same
publication and lifetime rules as objects.

The implementation is independent of VBCI and uses VRT class metadata for
tracing. VBCI retains its interpreter-local traversal while both runtimes are
validated against the same ownership semantics. See the
[VBCI to VRT migration policy](../../docs/architecture/vbci-vrt-migration.md).