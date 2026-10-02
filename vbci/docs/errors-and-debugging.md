# Errors and Debugging

Interpreter failures use private `Error` values carrying the active function
and instruction position. `Thread` prints the logical stack and asks `Program`
to resolve debug program counters to source files, lines, and function names.

Debug data is decompressed on demand. When source information is absent, VBCI
falls back to decoded function and class names.

Debug builds support instruction tracing and invariant checks. Run the
installed interpreter under the debugger so its associated runtime and format
artifacts match the compiler output.