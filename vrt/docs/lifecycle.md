# Runtime Lifecycle

`vrt_runtime_init()` initializes process-wide services. `vrt_program_init()`
registers one generated `verona_program` descriptor and initializes its
singleton state. `vrt_invocation_begin()` prepares one program invocation.

The native launcher initializes a logical thread, invokes generated entry code
inside the error boundary, and tears down invocation state before returning the
process exit code. Generated code provides `verona_program_entry()`; libvrt
provides the launcher and `set_exit_code()`.