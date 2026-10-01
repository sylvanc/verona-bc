# VBCI to VRT Migration Policy

VBCI currently implements interpreter-specific execution and memory management
while VRT is becoming the runtime used by native code. The long-term direction
is to share complete runtime semantics where doing so preserves both execution
models.

## Migration Invariant

Move one complete semantic subsystem at a time.

A subsystem is ready to move only when:

1. its public VRT contract expresses all semantics required by native code and
   VBCI;
2. ownership, lifetime, failure, and concurrency behavior are documented;
3. VRT tests cover the contract independently of VBCI;
4. VBCI integration tests cover the adapter or direct call boundary;
5. VBCI's replaced implementation and duplicate state can be removed in the
   same change or in a bounded compatibility step.

Until those conditions hold, VBCI retains its local implementation.

## Representation Boundary

Shared semantics do not require identical register representations.

VBCI's live `Value` is a boxed tagged union that includes interpreter-only
states such as register, field, array, cown, function, error, and invalid
values. VRT's `Value` is currently a non-owning view over a type and data
address. Neither is a replacement for the other.

The VBC format records encoded primitive and layout information. Those wire
identifiers belong to the neutral VBC contract, not to either live value type.
Adapters, when required, must be scoped to a concrete migrated subsystem and
must name the ownership conversion they perform.

## Canonical Ownership and References

**Current:** VRT owns the canonical native contracts for root and field
ownership, escape, and register/field/array references. `vrt::Value` is a
non-owning view. The internal ownership module performs root and field
retain/release and escape dispatch. `vrt_reference` records the referenced
storage, its content type ID, and the owner or frame lifetime that keeps that
storage valid.

The LLVM backend normalizes VIR `ArgMove` and `ArgCopy` before calling VRT:
moves transfer an existing root obligation, while copies retain through the
VRT ABI. Reference construction and exchange therefore consume already-owned
inputs. VRT alone implements owner lifetime, frame validation, load ownership,
and region-aware exchange.

**Migration:** VBCI retains its interpreter-local `Register`, `Value`, and
write-barrier implementation. Equivalent VBCI operations remain the
compatibility behavior until a later subsystem migration adapts bytecode
execution to the VRT contracts. The `reference` fixture runs one VIR
program through both execution backends to detect semantic divergence in the
interim.

## Logical-Frame Stack Storage

**Current:** Native stack object and array allocation is owned by VRT. Each
thread context provides stable-address chunk storage, and each logical frame
records the allocation and finalizer marks needed for normal return, raise
unwind, thread teardown, and tailcall reuse. Generated LLVM uses the public VRT
stack allocation entry points; it does not model Verona stack lifetime with
LLVM `alloca`.

VRT write barriers enforce the stack ownership boundary. Stack storage may
refer to values owned by the same frame or a surviving ancestor, but stack
values cannot be stored into regions or into older stack storage. Object,
array, and reference escape checks apply the same rule to return, raise, and
tailcall arguments. Cleanup finalizes managed contents before restoring
allocator marks. See [VRT Threads and
Frames](../../vrt/docs/threads-and-frames.md) for the runtime lifecycle.

**Migration:** VBCI retains its interpreter-local stack allocation and
ownership implementation. Shared VIR fixtures cover successful stack access
and rejected tailcall escape across VBCI and native VRT while the
representations remain separate.

## Collection, Freeze, and Immutable SCCs

**Current:** VRT independently implements native region/header collection,
finalization, Freeze, and immutable SCC lifetime. Its collector uses explicit
header and region work records, temporary former-owner guards for detached
mutable headers, fixed-point finalization, and header-before-region storage
release. Freeze performs a whole-graph read-only preflight before using
transient `Pending` state, publishes canonical ARC representatives, accounts
for region-crossing references, and applies ownership transitions deepest
first. The detailed native contracts are documented in
[Collection and Finalization](../../vrt/docs/collection-and-finalization.md)
and [Freeze and SCCs](../../vrt/docs/freeze-and-sccs.md).

**Migration:** VBCI retains its interpreter-local collector and Freeze
implementation. VRT does not depend on VBCI headers, interpreter values, or
program dispatch; it traces through native class metadata. The runtimes may
therefore use different worklist representations and lifetime guards while
preserving the same finalize-before-destroy, SCC, reference-accounting, and
ownership semantics. Sharing or adapting this subsystem requires a later
readiness review rather than a direct source-level port.

## Dependency Rules

- VBCI may depend on public VRT interfaces for migrated subsystems.
- VRT must not depend on VBCI implementation headers.
- VBC wire definitions must not depend on either runtime's live value type.
- Do not add a general `vbci/vrt_adapter` module. Broad adapters obscure which
  side owns lifetime, errors, and synchronization.
- LLVM code generation targets the public VRT ABI; files under the LLVM
  backend named `vrt` emit ABI calls and do not implement VRT.

## Candidate Subsystems

Examples still awaiting migration include VBCI reference execution,
type-layout lookup, object/array operations, regions and immutable lifetime,
cowns, scheduling, and failure reporting. This list is not a migration order.
Each subsystem needs a separate readiness review using the invariant above.

## Validation

Every migration must preserve:

- VBCI golden tests for interpreted execution;
- standalone VRT API and internal tests;
- LLVM-native fixtures that exercise the migrated ABI;
- builds with the LLVM backend both enabled and disabled when dependencies
  change.

Update this policy as VRT contracts mature. Use an ADR for a change to the
subsystem-at-a-time strategy or the representation boundary.