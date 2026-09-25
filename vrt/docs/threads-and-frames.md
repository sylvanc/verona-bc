# Threads and Frames

`vrt_thread_init()` binds a fresh logical Verona thread to the calling native
thread; `vrt_thread_deinit()` destroys that binding. Generated calls push
logical frames with `vrt_frame_enter()` and remove them with
`vrt_frame_leave()`.

Tail calls use `vrt_frame_reuse()` after lowering has moved arguments and
released other locals. Raise targets are stack locations associated with active
frames. Raise continuations preserve a typed encoded value while intermediate
frames are torn down.

Each frame also carries a native-storage epoch. Register references capture
that epoch with a pointer to generated variable storage. Reusing a logical
frame for an LLVM tailcall advances the epoch because the old native activation
and its allocas are about to disappear. Reference operations reject an inactive
frame or stale epoch.

Frame metadata references compiler-emitted `vrt_func` descriptors.