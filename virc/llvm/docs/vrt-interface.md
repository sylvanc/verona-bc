# VRT Interface

`vrt/declaration.cc` declares the C ABI calls required by generated modules.
These include error and frame operations, object and array allocation,
retention, freezing, lookup, bulk array operations, reference construction and
mutation, and callable entry lookup.

Declarations use the module data layout for machine-word operands and C calling
convention. Conflicting pre-existing declarations fail code generation.
`setjmp` is marked `returns_twice`; VRT raise entry points that do not return are
marked accordingly.

The `vrt/` implementation directory owns shared runtime adapters that are not
the lowerer for one VIR token. Frame entry and reuse, raise continuation setup,
and representation-directed retain, release, escape, and tail-call validation
are used across statement, call, and terminator lowering. Token-specific
emitters remain under `statements/`, `calls/`, or `terminators/` even when they
invoke VRT.

The ABI contract is owned by the installed headers under `include/vrt/` and
documented in [VRT ABI](../../../vrt/docs/abi.md).