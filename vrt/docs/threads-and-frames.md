# Threads and Frames

`vrt_thread_init()` binds a fresh logical Verona thread to the calling native
thread; `vrt_thread_deinit()` destroys that binding. Generated calls push
logical frames with `vrt_frame_enter()` and remove them with
`vrt_frame_leave()`.

Each thread context owns stable-address chunk storage for logical-frame stack
objects and arrays. A frame snapshots allocation and finalizer marks when it is
entered. Leaving a frame, unwinding after a raise, and thread teardown finalize
the frame's stack allocations, release their managed fields, restore the
storage marks, and then destroy the frame-local region. Raise cleanup completes
before control is transferred with `longjmp`.

Tail calls use `vrt_frame_reuse()` after lowering has moved arguments and
released other locals. Reuse finalizes and restores the outgoing activation's
stack storage while preserving the logical frame and its frame-local region.
Raise targets are stack locations associated with active frames. Raise
continuations preserve a typed encoded value while intermediate frames are
torn down.

Each frame also carries a native-storage epoch. Register references capture
that epoch with a pointer to generated variable storage. Reusing a logical
frame for an LLVM tailcall advances the epoch because the old native activation
and its allocas are about to disappear. Reference operations reject an inactive
frame or stale epoch.

Frame metadata references compiler-emitted `vrt_func` descriptors.