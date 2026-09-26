# VIRC Diagnostics

Shared diagnostic helpers live in `virc/support/diagnostics.h` and
`diagnostics.cc`. They construct Trieste error nodes with a message and source
AST, preserving location information for the driver.

`CompileResult::operator bool()` requires both a successful Trieste process and
an error-free `Compilation`. A pass that detects invalid VIR should report the
error through this shared path rather than terminating emission later.

Emitter-only failures remain local to the selected backend. The standalone
driver and VC surface the collected errors through their normal Trieste command
flow.