# Calls and Control Flow

Static calls resolve a generated LLVM function. Dynamic calls use emitted
lookup metadata and VRT callable entry resolution. Tail calls reuse the current
runtime frame when their lowering supports it.

Each VIR label becomes an LLVM basic block. Conditional and jump terminators
branch to the mapped block; return and raise terminators leave normal control
flow through their dedicated lowering paths.

Generated functions enter, leave, or reuse VRT frames. Raise handling
materializes the source value, transfers its type ID and encoded storage
through the public frame continuation API, and reloads it at the target, as
described in [VRT Threads and Frames](../../../vrt/docs/threads-and-frames.md).