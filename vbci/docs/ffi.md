# FFI

`Dynlib` opens named native libraries and resolves symbols. `Symbol` stores the
VBC type IDs and libffi layouts for parameters and results, prepares fixed or
variadic call interfaces, and invokes resolved function pointers.

VBCI marshals primitive values directly. Objects and arrays pass pointers to
their data representation. Callback trampolines marshal C arguments into
interpreter registers and write the Verona result back through libffi.

User-facing declarations and wrapper syntax are documented in
[FFI](../../vc/docs/17-ffi.md).