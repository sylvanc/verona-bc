---
name: vbci-memory
description: Verona interpreter (vbci) memory management model — regions, reference counting, write barriers, dragging, cowns, and finalization. Use when working on or debugging vbci runtime memory management code.
---

# vbci Memory Management

## Overview

The Verona interpreter uses a region-based memory model with reference counting. Objects live in regions, regions form strict trees (single parent, single entry point), and cowns provide a forest of independent region trees for concurrency.

## Object Locations

Every object/array has a `Location` (encoded as a tagged `uintptr_t`):

| Location | Meaning | RC type | Collected by |
|----------|---------|---------|-------------|
| **Stack** | Stack-allocated in a frame | No RC | Frame teardown (destruct) |
| **Frame-local** | In a frame's embedded RegionRC | Object RC | RC=0 collection, or `free_contents` for cycles |
| **Region** | In a heap RegionRC | Object RC | RC=0 collection, or `free_contents` for cycles |
| **Immutable** | Frozen, shareable | ARC (atomic) | ARC=0 collection |
| **Immortal** | Primitive values, constants | No RC | Never collected |

## Two Independent RC Systems

### Object RC (`Header::rc`)

Counts **all** incoming references to an object, regardless of source (registers, fields, anywhere). When RC hits 0, the object is collected individually via `collect()`. The region's stack_rc is irrelevant to individual object collection.

- `inc<needs_stack_rc>()`: increments object RC; if `needs_stack_rc`, also increments the region's stack_rc
- `dec<needs_stack_rc>()`: decrements object RC; if RC hits 0, queues for collection via `collect()`

The `needs_stack_rc` template parameter indicates whether the reference is from a register/stack/frame-local context (true) or from a field within a region (false).

### Region Stack RC (`Region::stack_rc`)

Counts references into the region from **outside** the region's own object graph — specifically from:
- Frame registers
- Fields of stack-allocated or frame-local objects/arrays

This keeps entire regions alive. A region can be collected (freed as a whole) when:
- It has **no parent** (not parented to another region, not cown-owned)
- Its **stack_rc is 0** (no external references)

Stack_rc is NOT per-object — it's per-region.

## Regions

Regions are containers of objects that form **strict trees**:
- Each region has at most one parent region
- There is exactly one entry point from a parent to a child region
- Cowns can own regions, creating a **forest** of region trees

### Region Types
- **RegionRC**: Reference-counted region with a `headers` set tracking all contained objects
- Regions track: `stack_rc`, `parent` (with tag bits for parent type), `finalizing` flag

### Region Parenting
The `parent` field encodes the owner:
- `0`: no owner (root region, collectible if stack_rc == 0)
- Pointer with no tag bits: parent is another Region
- `parent_tag_cown`: owned by a cown
- `parent_tag_frame_local`: frame-local region

## Frame Teardown

Each frame has:
- **Stack allocations**: objects allocated with `Op::Stack` on the frame's stack area
- **Frame-local region**: an embedded `RegionRC` for objects created with `Op::New`

Teardown order:
1. **`frame->drop()`**: Clears all registers. RC decrements may trigger individual object collection via `collect()`.
2. **Phase 1**: Run finalizers on stack objects (via `run_sync` with readonly reference)
3. **Phase 2**: Destruct stack objects (drop field references, no finalizer)
4. **`free_contents()`**: Finalize and deallocate everything remaining in the frame-local region. This handles **cyclic garbage** — objects whose RC never hit 0 because they only have internal references within the region.

## Dragging

When an object in a frame-local region needs to escape (return to parent frame, be stored in a region, be sent to a cown), it is **dragged** via `drag_allocation()`:

1. DFS traversal from the root object to find all reachable frame-local objects
2. Track internal RC (edges within the dragged set) vs external RC
3. Check that child regions can be parented (no existing owner, no cycles)
4. Move all objects to the destination region, adjusting RCs and setting parents

The `ignore_parent` parameter allows drags during exchange: when the outgoing value's region will be unparented by `apply_out`, the drag can treat that region as unowned without actually unparenting it first.

## Write Barrier (`writebarrier.h`)

The write barrier handles stores (field writes, array element writes) with a prepare/apply pattern:

```
write_ops<is_move>()
    .prepare_store(store_loc)     // where the store target lives
    .prepare_out(prev_loc)        // where the outgoing (old) value lives
    .prepare_in(next_loc)         // where the incoming (new) value lives
```

Then:
- `apply_in()`: executes drag, writes value, adjusts incoming RC/stack_rc/parent
- `apply_out()`: adjusts outgoing RC/stack_rc, unparents outgoing region

Key operations planned during prepare:
- **Incoming**: drag (frame-local → region), RC inc/clear, stack_rc adjust, region parent
- **Outgoing**: RC dec, stack_rc adjust, region unparent
- `drop()`: simplified version for field drops during finalization (no incoming value)

### Ordering constraint
`apply_out` runs AFTER `apply_in`. When the incoming drag needs the outgoing region to be unparented first, `ignore_parent` on `drag_allocation` resolves this without reordering.

## Collection (`collect.cc`)

The `collect()` function handles deferred deallocation to avoid re-entrancy issues:

- First call: sets `in_collection = true`, processes worklist
- Nested calls (from `deallocate()` → `finalize()` → field drops → more RC=0): add to worklist, return immediately
- Worklist processes until empty, then `in_collection = false`

Each worklist item calls `deallocate()` which calls `finalize()` then frees the memory.

## Cowns

Cowns (`verona::rt::VCown`) own a value and optionally a region:
- `add_reference()`: stores a value in the cown, potentially dragging frame-local to a fresh region and setting cown ownership
- `exchange()`: swaps the cown's content, handling region re-parenting with rollback on failure
- Cown-owned regions have `parent = parent_tag_cown`

## Readonly Values

Readonly values always come from a readonly cown (via `Read` op or behavior cown args with read-only slots). Key properties:
- **Cannot be mutated**: stores through readonly refs raise `BadStoreTarget`
- **Cannot have new field references**: no object/array can create a new field pointing to a readonly value
- **No object/array RC**: `reg_inc`/`reg_dec` skip object RC and stack_rc for readonly objects/arrays
- **Cowns still need RC**: readonly cown values (`ValueType::Cown`) still need `cown->inc()`/`cown->dec()` — the cown itself is ref-counted independently of its content's readonly status
- **CownRef needs no RC**: `CownRef` values exist only as behavior arguments and are alive for the behavior's lifetime — no RC needed

## Finalization

Objects with a `final` method have their finalizer run during collection/teardown:
- Called with a **read-only** reference to the object (prevents resurrection)
- Finalizers must not mutate — stores through readonly refs raise `BadStoreTarget`
- `Object::finalize()`: runs finalizer via `run_sync()`, then drops all field references via `writebarrier::drop()`
- `Object::destruct()`: drops field references without running the finalizer (for stack objects after Phase 1)

### Finalization ordering
The collector uses a two-phase approach: Phase 1 finalizes all objects (running finalizers, dropping field references), Phase 2 frees memory. This prevents use-after-free when finalizers reference sibling objects — all finalizers run before any memory is freed.

## Sendability

An object is sendable if its region:
- Has no owner (no parent region, no cown owner)  
- Has stack_rc == 1 (only the current register references it)

Frame-local objects are dragged to a fresh region first, then the region is checked for sendability. This ensures data-race freedom — only objects with exclusive ownership can be sent to behaviors.

## Freezing (`freeze.cc`)

Freezing converts a reachable subgraph from mutable region objects to immutable objects with ARC tracking. The region survives with any unfrozen objects.

### Two entry points
- **`freeze(Header* root)`**: Freezes reachable subgraph in a heap region. Handles frame-local roots by delegating to `freeze_local`. Returns true on success.
- **`freeze_local(Header* root)`**: Freezes frame-local objects in place, then delegates discovered heap sub-regions to `freeze()` in phase 2.

### SCC detection with rc-transfer
Uses Tarjan-style DFS with union-find. During SCC detection, `scc_union(child, rep)` transfers `child.rc - 1` to rep (subtracting the tree edge). Back edges decrement `rep.rc`. At SCC completion, `set_arc(rep->get_rc())` gives the exact external ref count without needing separate inc_arc calls.

Union-by-address (using `uintptr_t` of header as rank) with path compression gives O(α(n)) amortized.

### Stack_rc adjustment
After freezing, the region's stack_rc must be adjusted to account for stack references that now point to immutable objects:

```
stack_adjustment = arc_sum - frozen_cross - unfrozen_to_frozen
```

- `arc_sum`: sum of all SCC arcs (total external refs to frozen objects)
- `frozen_cross`: cross-edges between newly-frozen SCCs (tracked via `frozen_set`)
- `unfrozen_to_frozen`: field refs from surviving region objects to newly-frozen objects

### Sub-region handling
When the freeze DFS discovers an entry point to a sub-region, it pushes it to a worklist. After freezing the parent region, sub-regions are processed. Ownership is cleared only when `root == region->get_entry_point()`.

### Entry point tracking
`Region` stores a `Header* entry_point` set during `set_parent()` and `set_cown_owner()`. This is needed to:
- Assert the single-entry-point invariant during freeze
- Guard ownership clearing (only clear when freezing the entry point)

### Location states during freeze
| State | Location | rc/arc field | Meaning |
|-------|----------|-------------|---------|
| UNMARKED | Region ptr | rc = field refs | Normal mutable, not yet visited |
| PENDING | Pending tag | rc = accumulated external refs | On DFS path, SCC candidate |
| SCC_PTR | SCC ptr to rep | unused | Union-find member, points toward root |
| IMMUTABLE | Immutable tag | arc = external ref count | Frozen SCC root |

### Immutable object lifecycle
- `field_inc()` on immutable → `arc++`
- `field_dec()` on immutable → `arc--`; if 0, `collect_scc()` walks all SCC members via SCC_PTR chains, finalizes, and frees
- `reg_inc()`/`reg_dec()` on immutable → only `field_inc()`/`field_dec()` (no stack_rc)

## Key Files

| File | Purpose |
|------|---------|
| `header.h` | Base Header class: RC, location, region access, inc/dec |
| `region.h` | Region struct: stack_rc, parent, ownership |
| `region_rc.h/cc` | RegionRC: headers set, insert/remove/rfree/free_contents |
| `location.h/cc` | Location encoding, `drag_allocation` |
| `writebarrier.h` | Write barrier: prepare/apply pattern for stores |
| `collect.h/cc` | Deferred collection worklist |
| `object.h` | Object: init, finalize, destruct, deallocate |
| `array.h` | Array: element access, finalize, deallocate |
| `cown.h` | Cown: add_reference, exchange |
| `value.h/cc` | Value union type: RC forwarding, location queries |
| `register.h` | Register wrapper: ownership semantics |
| `thread.h/cc` | Frame management, teardown, `run_sync` |
