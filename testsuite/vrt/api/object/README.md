# VRT object runtime fixture

This fixture exercises the public object allocation, ownership, lookup, and
escape ABI together with the private representation needed to validate its
effects.

## Coverage

- Frame-local `new`, logical-frame `stack`, existing-region `heap`, and
  fresh-region allocation for nominal objects, including initialized fields
  and region placement.
- Distinct empty-object allocation in frame-local, stack, RC-region, and arena
  storage, both with and without separately provisioned singleton metadata.
- Compiler-emitted class, field, method, singleton, and type metadata.
- Data/header conversion, class IDs, retain/release, and collection.
- Explicit immortal singleton identity and method-table lookup.
- Graph relocation when an object is returned or raised across frame teardown.

## Non-goals

Array behavior, invalid allocation inputs, and internal region algorithms are
covered by separate fixtures.
