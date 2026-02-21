# High-Level Language Experiment

## To Do

Names.
- Allow looking down a type parameter.
- Can we look down an algebraic type (via an alias)?

Reification.
- References.
- Encode shapes as type aliases of all implementing concrete types.
- Post reification type checking.

Sub-typing.
- Better contradiction for type aliases.
- Intersections can fulfill shapes.
- Check recursion on type alias.
  - Does `alias[alias[A]]` cause both `A` and `alias[A]` to be bound to the same type parameter?

Type inference.
- Does backwards refinement work for things that aren't primitives?
- Infer lambda return as union of all possible return types.
- Determine `when` type from the `when / Rhs` function type.

Check use before definition in `let x = ... x ...`.

Calls.
- Try (rename to `catch`?), sub-call.
- Raise, throw.

Lambdas.
- A free `var` is captured by reference. The lambda must be `stack` allocated.

Patterns for lambdas.
- If a lambda can be a pattern that returns `nomatch`, then `if` with a type-test lambda is the same as invoking the lambda with the value.
- If a lambda can be a pattern, pattern matching becomes a series of `else`.
- A pattern can be any object that implements `==`.
  - Auto-wrap it in some Pattern container to get logical operators and a type test (before calling `==`) that returns `nomatch`?
- `!`, `&`, `|` for patterns. Can do this as methods on a common Pattern structural type.

Tuples:
- Destructing when the tuple is too short throws an error. Should it?
- Destructing when the tuple is too long ignores the extra elements. Should it?
- Flatten a tuple into another tuple.

Expressions:
- Partial application, `_`.

Syntax:
- Region creation.

Structure:
- Auto create, member conflict.
- Can auto-RHS conflict with default arguments? Seems yes.

Builtin:
- String. Just so that string literals have a type that isn't `array[u8]`.

Packages:
- CLI.
  - Needs a map, so do an RB tree or hash map.
    - A map needs structural types.
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
- Control over FFI and built-in access?

Encode `ValueParam` as types.
- Need an "equivalence" relation at compile time for invariant sub-typing.
- This may require execution during compilation.

Types:
- Turn function types into structural types.
- IR types for: union, intersection, tuple, function.
  - IR tuple type could be `[dyn]` of correct size with elements that type check.
- Can type parameters take type arguments?

Optimization:
- Optimize dynamic calls as static when there's a known type for the receiver.
- Compile time evaluation.
