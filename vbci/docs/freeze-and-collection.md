# Freeze and Collection

`freeze.cc` makes the reachable portion of a region deeply immutable and
builds immutable reference-count state across strongly connected components.
It handles nested regions, cross edges, cown ownership, and frame-local roots.

`collect.cc` and region implementations reclaim unreachable mutable state and
run queued finalizers at the ownership boundary. Freeze and collection share
header and region invariants but are separate operations.

**Migration:** VRT implements native equivalents independently. The sequencing
rules are in the [runtime migration policy](../../docs/architecture/vbci-vrt-migration.md).