# Pin Fixture

## Purpose

This fixture verifies the VRT object and array pin/unpin ABI and the root
obligation it creates for external users.

## Scenarios

- Pinning a frame-local object or array relocates its reachable allocation to a
  fresh heap RC region before adding the pin reference.
- Releasing the original root leaves the pin as the region's final external
  reference, and unpinning consumes that obligation and permits collection.
- Immutable objects use the same pin/unpin API with atomic reference counts.
- Stack allocations reject pinning with `BadStackEscape` and unwind normally.

## Runtime Boundary

The fixture calls the public object and array ABI, then uses private header and
region state only to verify relocation and reference accounting.

## Non-Goals

Cown pinning is not part of VRT until VRT owns a cown subsystem. LLVM lowering
is covered by compiler integration fixtures.
