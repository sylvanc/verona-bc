# LLVM reference lowering

This fixture runs through both the VBCI bytecode path and the LLVM-native VRT
path.

## Coverage

- `RegisterRef`, `FieldRef`, `ArrayRef`, and `ArrayRefConst`;
- `Load` and `Store`;
- reference arguments and returns;
- copied and moved reference owners;
- a reference passed through a static tailcall;
- a field reference transported through a language raise.

## Native VRT coverage

The generated native program constructs VRT references, loads and exchanges
their contents, retains and releases reference owners, validates a reference
before a tailcall, relocates a returned field-reference owner, and transports a
multiword reference through typed raised-value storage.

## Non-goals

Readonly cown references, dynamic values, reference-valued object fields and
array elements, and invalid-reference diagnostics remain covered by VRT API
fixtures or later native fixtures.
