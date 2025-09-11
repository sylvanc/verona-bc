# High-Level Language Experiment

## Current Work

Move the `TypeId` for Header into Class.
In Header and `Cown`, store a `uint32_t` index into a Class vector.
- This allows arrays and `cown` to also have a Class associated with them.
This leaves `ref`. Keep a map of `TypeId` to Class for `ref`. Also use this to populate the class index for objects, arrays, and `cown`.

Reification.
- Arrays.
  - Need to reify array as a primitive. There's no `new`, so it doesn't get generated.
    - Problem: need a separate method table for every type of array.
    - This is because apply returns a T. Is it ok for this to be `dyn`?
    - Separate `vtable` lookup for arrays based on `TypeId`?
      - Could do the same for `ref T` and `cown T`.
      - `Value::type_id` gives a useful value for `array`, `ref`, and `cown`.
  - Emit as `Array T` IR type.
- Don't fail on method instantiation failure.
  - Mark as "delete on error".
  - On completion of `run`, check if it contains errors.
- Need `ref` primitive, covering `FieldRef`, `ArrayRef`, and `RegisterRef`? This would allow `ref` load to be `*x` or `x()`.
  - Same problem as array: need a separate method table for every type of `ref`.
- Implement `cown`.
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
- Rename shadowed variables.
- Need to be able to load a `localid`.
  - This is for when a local is of type `ref T`.
  - Keyword? Or a method on `ref`?

Control flow:
- For, When.

Tuples:
- Destructing when the tuple is too short throws an error. Should it?
- Destructing when the tuple is too long ignores the extra elements. Should it?
- What's the syntax for tuple element reference?
  - `apply` method on `array`?
- Flatten a tuple into another tuple.

Calls:
- Try, sub-call.

Expressions:
- Only allow `:::` in `std::builtin`.
- Array operations for `std::builtin`.
- Partial application, `_`.
- Unescape strings.

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
- Array.
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

## Syntax

```rs

top = class*

typename = ident typeargs? ('::' ident typeargs?)*
typearg = type | '#' expr
typeargs = '[' typearg (',' typearg)* ']'

// No precedence, read left to right.
type = typename
     | type '|' type // Union.
     | type '&' type // Intersection.
     | type ',' type // Tuple.
     | type '->' type // Function. Right associative.
     | '(' type ')' // Parentheses for grouping.

typeparam = ident (':' type)? ('=' type)?
typeparams = '[' (typeparam (',' typeparam)*)? ']'

// Classes.
class = ident typeparams? '{' classbody '}'
classbody = (class | import | typealias | field | function)*
import = 'use' typename
typealias = 'use' ident typeparams? '=' type
field = ident (':' type)? ('=' expr)?

// Functions.
function = ident typeparams? params (':' type)? '{' body '}'
params = '(' param (',' param)* ')'
param = ident (':' type)? ('=' expr)?
body =
     ( import | typealias | 'break' | 'continue' | 'return' expr
     | 'raise' expr | 'throw' expr | expr
     )*

// Expressions.
lambda = typeparams? params (':' type)? '->' (expr | '{' body '}')
       | '{' body '}'
qname = typename '::' (ident | symbol) typeargs?
      | ident typeargs

// Expression binding.
strong = '(' expr ')' // exprseq
       | 'let' ident (':' type)?
       | 'var' ident (':' type)?
       | literal
       | expr (',' expr)+
       | lambda
       | ident
       | expr '.' (ident | symbol) typeargs? // method
       | qname // function
       | qname exprseq // static call
       | method exprseq // dynamic call
       | 'if' expr lambda
       | 'while' expr lambda
       | 'for' expr lambda
medium = apply expr // extend apply
       | expr expr // apply
weak   = 'ref' expr
       | 'try' expr
       | symbol typeargs? expr // dynamic call
       | expr symbol typeargs? expr // dynamic call
       | expr 'else' expr
       | expr '=' expr
       | apply
         // qname: static call
         // method: dynamic call
         // other: dynamic call of `apply`

```
