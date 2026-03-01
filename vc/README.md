# High-Level Language Experiment

## To Do

FFI.
- Run code before scheduler start and after scheduler end for initialization and cleanup.
  - Needed for `uv`, but not for `openssl` 3+.
  - Register a termination callback? Doesn't have to be a `libffi` closure, because we can call a byte code function.
- Expose a version of `add_external` and `remove_external`.
  - `uv` needs to be able to run code on these, in order to poke the event loop.
  - Allow registering a callback for this. Keep a list of them. Also doesn't have to be a `libffi` closure.
- Create C callbacks with `libffi`. Expose this as a core interpreter mechanism.
  - How do we manage this memory?
  - Can we demand callbacks are isolated regions or immutable?
- Platform dependent code for dealing with libraries?
- Remove `vbci` dependency on `uv` and `openssl`.

Reviews.
- A Verona writing skill? `vc` project scaffold that adds the skill and a project-specific CLAUDE.md?
- Can we review and improve compiler error messages?

Tuples.
- Flatten a tuple into another tuple.
- Allow a splat in a for loop lambda signature?

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
