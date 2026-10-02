# VRT object runtime fixture

This fixture exercises the public object allocation, ownership, lookup, and
escape ABI together with the private representation needed to validate its
effects.

## Coverage

- Frame-local `new`, existing-region `heap`, and fresh-region allocation for
  nominal objects, including initialized fields and region placement.
- Compiler-emitted class, field, method, singleton, and type metadata.
- Data/header conversion, class IDs, retain/release, and collection.
- Singleton allocation behavior and method-table lookup.
- Graph relocation when an object is returned or raised across frame teardown.

## Non-goals

Array behavior, invalid allocation inputs, and internal region algorithms are
covered by separate fixtures.
