# Calls and Raises

Calls push a `Frame` with a destination register, local base, saved stack
position, region, and raise target. Returns tear down the frame and transfer the
result to its caller. Tail calls reuse the logical call position after releasing
the current frame's non-argument state.

Dynamic calls resolve methods from the receiver. `TryCallDyn` returns an empty
value when lookup or argument checking fails; ordinary dynamic calls report an
error. Raises unwind frames to a validated stack location and preserve the
raised value across teardown.