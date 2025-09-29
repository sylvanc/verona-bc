# High-Level Language Experiment

## Current Work

Check use before definition in `let x = ... x ...`.

Concurrency.
- When pushes function pointer, optional closure data, then cowns.
  - Change the VC emitter.
- Test `when` with multiple `cown`.

Reification.
- Don't fail on method instantiation failure.
  - Mark as "delete on error".
  - On completion of `run`, check if it contains errors.
- Test type aliases, make sure cycles are rejected.
- Optimize dynamic calls as static when there's a known type for the receiver.

Lambdas.
- A free `var` is captured by reference. The lambda must be `stack` allocated.
- Type parameters can be "free" in a lambda.

## To Do

RHS functions for `ValueParam`? Or treat them like `ParamDef`?
- Treat them as much as possible as a `TypeParam`.

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
- What's the syntax for tuple element reference?
  - `apply` method on `array`?
- Flatten a tuple into another tuple.

Calls:
- Try (rename to `catch`?), sub-call.
- Raise, throw.

Expressions:
- Only allow `:::` in `std::builtin`.
- Partial application, `_`.
- Unescape strings. Currently happens in `validids`.

Syntax:
- How to put a dynamic type in a tuple or function type?
- `where` clause on a type alias.
- Zero-argument function calls.
  - How do we call a zero-argument lambda?
  - How do we refer to a zero-argument function without calling it?
- Braces and else: it doesn't work if there's a comment in between.
- Object literals: `new { ... }`.

Structure:
- Default field values.
- Auto create, member conflict.
- Can auto-RHS conflict with default arguments? Seems yes.
- Could allow `ident::name` (lookup, no call).
  - Like `ident.name`, but no first argument binding.
- Compile time evaluation.

Standard library:
- Primitive type conversions.
- String.
- `stdin`, `stdout`, `stderr`.

Packages:
- CLI.
- Network.
- Hash map, hash set, ordered map, ordered set, list, deque, vector, span.
- Persistent collections.
- Sort.

Software engineering:
- Code reuse.
- Public, private.
- Use libgit2 for fetching dependencies.
- FFI.

Types:
- IR types for: union, intersection, tuple, function.
  - IR tuple type could be `[dyn]` of correct size with elements that type check.
- Structural types.
- Turn function types into structural types.
- Can type parameters take type arguments?
