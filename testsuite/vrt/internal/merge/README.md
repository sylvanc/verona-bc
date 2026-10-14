# VRT merge coverage

This fixture characterizes the native mutable-region merge transaction.

## Coverage

- same-region and two-frame-local no-op behavior;
- frame-local graph relocation into an RC region;
- object/array RC-region unification and root-count transfer;
- direct child-region reparenting;
- selecting an already-owned region as the destination;
- failure-atomic rejection when both regions are owned;
- failure-atomic rejection of ancestor/descendant and arena merges;
- stack-allocation escape rejection.

## Native VRT coverage

The fixture checks object locations, region membership, stack reference counts,
parent and entry-point metadata, and public error codes before releasing every
successful or rejected graph.

## Non-goals

LLVM statement lowering is covered by the compiler merge fixture. Cown merge is
unsupported because VRT does not yet own a cown subsystem.
