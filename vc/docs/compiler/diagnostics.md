# VC Diagnostics

VC reports parse and rewrite failures through Trieste error nodes. A diagnostic
contains an error message and the offending AST so processing can collect more
than one local failure before reporting the result.

Frontend passes should attach the most specific available source location and
must not encode backend-specific failures. Errors discovered only after
reification belong to VIRC's
[diagnostic boundary](../../../virc/docs/diagnostics.md).

Use `-p <pass>` to stop after one pass and `--dump_passes=<directory>` to write
intermediate trees. User-facing commands are documented in
[Toolchain Usage](../21-toolchain-usage.md).