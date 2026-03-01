# High-Level Language Experiment

## To Do

FFI.
- Can `ffi::callback` interact with reference counting better?
- `init` and `fini` in library imports.
- Expose a version of `add_external` and `remove_external`.
  - Allow registering a callback for this. Keep a list of them. Also doesn't have to be a `libffi` closure.
- Platform dependent code for dealing with libraries?

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
