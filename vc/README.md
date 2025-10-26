# High-Level Language Experiment

## Current Work

Compilation path.
- Local builds have none. Set this as a `vbci` flag.
- Release builds encode a git repo and ref?

Check use before definition in `let x = ... x ...`.

Reification.
- Don't fail on method instantiation failure.
  - Mark as "delete on error".
  - On completion of `run`, check if it contains errors.
- Test type aliases, make sure cycles are rejected.

## To Do

Dependencies.
- Not picking up new commits when using `main` as the ref.

Calls.
- Try (rename to `catch`?), sub-call.
- Raise, throw.

Lambdas.
- A free `var` is captured by reference. The lambda must be `stack` allocated.
- Type parameters can be "free" in a lambda.

Patterns for lambdas.
- If a lambda can be a pattern that returns `nomatch`, then `if` with a type-test lambda is the same as invoking the lambda with the value.
- If a lambda can be a pattern, pattern matching becomes a series of `else`.
- A pattern can be any object that implements `==`.
  - Auto-wrap it in some Pattern container to get logical operators and a type test (before calling `==`) that returns `nomatch`?
- `!`, `&`, `|` for patterns. Can do this as methods on a common Pattern structural type.

Assign:
- Rename shadowed variables in `if`, `while`.

Tuples:
- Destructing when the tuple is too short throws an error. Should it?
- Destructing when the tuple is too long ignores the extra elements. Should it?
- Flatten a tuple into another tuple.

Expressions:
- Partial application, `_`.

Syntax:
- Braces and else: it doesn't work if there's a comment in between.
- Literals.
  - Grouping characters in numbers: `1_000_000`.
  - Unescape strings. Currently happens in `validids`.
- Region creation.

Structure:
- Auto create, member conflict.
- Can auto-RHS conflict with default arguments? Seems yes.
- Compile time evaluation.

Builtin:
- String. Just so that string literals have a type that isn't `array[u8]`.

Packages:
- Arguments, environment.
- `stdin`, `stdout`, `stderr`.
- File system.
- CLI.
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

RHS functions for `ValueParam`? Or treat them like `ParamDef`?
- Treat them as much as possible as a `TypeParam`.

Types:
- Structural types.
  - `self` type? Special subtype rules for `self` on the receiver?
  - Turn function types into structural types.
- Infer type arguments in expressions.
- IR types for: union, intersection, tuple, function.
  - IR tuple type could be `[dyn]` of correct size with elements that type check.
- Can type parameters take type arguments?

Optimization:
- Optimize dynamic calls as static when there's a known type for the receiver.
