# High-Level Language Experiment

## To Do

Reification.
- Can type parameters be encoded as type aliases in the reified type?
- Type names in FFI symbols must not contain type parameter references.
  - Find symbols from FFI calls rather than reifying all of them.
- Make function reification iterative instead of recursive.
  - Too much stack usage.
  - Can do the signature right away, to get the return type.

Type inference.
- Create a `TypeVar` for integer and float literals?
  - Upper bounds is the union of literal types.

- References, load, store.
- Lookup, dynamic call.
- Determine type arguments from context:
  - Call.
  - Dynamic call.
- Determine `when` type from the `when / Rhs` function type.

Check use before definition in `let x = ... x ...`.

Type aliases.
- Test type aliases, make sure cycles are rejected.

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
- Short-circuiting.
```rs
shape to_bool
{
  apply(self: Self): bool;
}

bool
{
  apply(self: bool): bool
  {
    return self;
  }

  &(self: bool, other: to_bool): bool
  {
    if self { other() } else { false }
  }

  |(self: bool, other: to_bool): bool
  {
    if self { true } else { other() }
  }
}
```

Syntax:
- Braces and else: it doesn't work if there's a comment in between.
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

RHS functions for `ValueParam`? Or treat them like `ParamDef`?
- Treat them as much as possible as a `TypeParam`.
- Could encode them as types, since they have to be statically compiled. So uses of them "just" call create on the type.
- Would need an "equivalence" relation at compile time for invariant sub-typing.

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
- Compile time evaluation.
