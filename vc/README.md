# High-Level Language Experiment

## To Do

Structure.
- Control flow blocks should be simpler.
- What type do control structures have?

Pattern matching.
- `TryCallDyn` in case lambdas.

Tuples.
- Test case for an array of tuples.
- Destructing when the tuple is too short throws an error. Should it?
- Destructing when the tuple is too long ignores the extra elements. Should it?
- Flatten a tuple into another tuple.

Names.
- Allow looking down a type parameter.
- Can we look down an algebraic type (via an alias)?

Types.
- Better contradiction for type aliases.
- Intersections can fulfill shapes.
- Check recursion on type alias.
  - Does `alias[alias[A]]` cause both `A` and `alias[A]` to be bound to the same type parameter?
- Can type parameters take type arguments?

Semantics:
- Region creation.
- Freezing.
- `final` functions.

Packages:
- Remove `vbci` dependency on `uv` and `openssl`, load those from packages.
  - `openssl` 3+ no longer needs initialization.
  - `uv` needs to be initialized (before the scheduler starts) and cleaned up (after the scheduler finishes).
- CLI.
  - Needs a map, so do an RB tree or hash map.
  - Needs arguments, environment variables.
  - Needs console I/O to print usage and errors.
- `stdin`, `stdout`, `stderr`.
- File system.
- Network.
- Hash map, hash set, ordered map, ordered set, list, deque, vector, span.
- Persistent collections.
- Sort.

Software engineering:
- Public, private.

Security:
- Integrate package management with signed and transparent statements and policies?
- LFI sandboxing?
- Control over FFI?

Encode `ValueParam` as types.
- Need an "equivalence" relation at compile time for invariant sub-typing.
- This may require execution during compilation.

Optimization:
- Optimize dynamic calls as static when there's a known type for the receiver.
- Treat parameter types that are shapes as implicit type parameters.
- Compile time evaluation.
