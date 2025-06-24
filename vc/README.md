# High-Level Language Experiment

## To Do

Assign:
- Need to be able to load a `localid`.
  - Keyword? Or a method on `ref`?

ANF:
- Break, continue need to deal with loop labels.
  - Since loops are expression, should `break` and `continue` have values?
- `if` with a lambda that takes arguments is a type test.
  - Allow type test version of `for`?
  - Allow a pattern, not just a type test?
  - If all lambdas are patterns that return `nomatch`, then `if` with a lambda is the same as invoking the lambda with the value.
- For, When.
- Type arguments on calls?
- Figure out copy and move.
  - Just do copy for now, figure out move later.
  - Could `vbcc` figure out move for us?

Tuples:
- Destructing when the tuple is too short throws an error. Should it?
- Destructing when the tuple is too long ignores the extra elements. Should it?
- A `tuple` is a `[dyn]`?
  - What's the syntax for tuple element reference?
    - `apply` method on `array`?
  - Can we do IR-level type checking?
    - IR tuple type demands `[dyn]` of correct size with elements that type check.
- Flatten a tuple into another tuple.

Calls:
- Try, sub-call, tail-call.
- Set-once for `let`. Set-before-use for `let` and `var`.

Expressions:
- Only allow `:::` in `std::builtin`.
- Array operations for `std::builtin`.
- Partial application, `_`.
- Pattern matching.
  - If a lambda can be a pattern, this becomes a series of `else`.
- Generators with `yield`.

Syntax:
- Braces and else: it doesn't work if there's a comment in between.

Structure:
- Auto create, member conflict.
- Lambdas to classes.
  - Mark free variables in lambdas.
  - A free `let` is captured by value. All the free `let` variables are used to determine where the lambda is allocated.
  - A free `var` is captured by reference. The lambda must be `stack` allocated.
- Could allow `ident::name` (lookup, no call).
  - Like `ident.name`, but no first argument binding.
- Compile time evaluation.

Reachability and flattening.
- Find all reachable classes and functions with their type arguments.
- Flatten the classes and functions.
- Expand `QName` and `TypeName` to the flattened names.

Standard library:
- Primitive type conversions.
- Array.
- String.

Packages:
- Code reuse.
- Public, private.
- Use libgit2 for fetching dependencies.
- FFI.

Types:
- Structural types.
- Type assertions.
- `where` clause instead of `T1: T2 = T3`?
  - Then a value parameter can be `ident: type`.
- Name resolution.
  - Look down through type aliases via union and intersection types.
  - Look down through type parameters. Need an upper bounds.
    - Restrict to type parameters as the first path element?
    - Still causes a lookup problem:
      1. Class `C` has type parameter `T`.
      2. Class has type alias `A = T::U`.
      3. `C[X]::A` should resolve to `X::U`, which means tracking.
- Infer the location for everything.
  - Separate expressions to add region constraints.
  - `a in b`, `a @ b`, or some such.

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
