# High-Level Language Experiment

## To Do

Optimize.
- Array fill/copy is slow.

Freezing.
- Need freeze syntax and a freeze `vbci` operation.
- Can we freeze an object instead of a region, extracting from the region as we go?

Parser issue: `{}, ...` terminates the group the `{}` are in before the `,` starts a List, creating a weird structure.

String literals should be immortal, not fresh allocations.

Review `iowise`.
- Buffer final can't call flush, that relies on mutation.
- Dispose pattern?

Semantics:
- Region creation.

FFI.
- Can `ffi::callback` interact with reference counting better?
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

Packages:
- Asynchronous `stdin`.
- Hash map, hash set, ordered map, ordered set, list, deque, vector, span.
- CLI.
  - Needs a map, so do an RB tree or hash map.
  - Needs console I/O to print usage and errors.
  - Needs arguments, environment variables.
- File system.
- Network.
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
