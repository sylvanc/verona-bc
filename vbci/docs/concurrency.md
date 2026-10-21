# Concurrency

VBCI integrates with the Verona runtime scheduler. `when` instructions package
arguments and queue a behavior against cowns; each behavior executes with its
own interpreter `Thread` state.

The scheduler is initialized before library and memo initializers because those
paths may queue behaviors. Scheduler completion precedes FFI finalizer
callbacks, so teardown callbacks cannot depend on newly queued work making
progress.

Language semantics are documented in
[Concurrency](../../vc/docs/15-concurrency.md).