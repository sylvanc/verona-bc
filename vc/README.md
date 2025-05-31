# Syntax Experiment

Infer the location for everything.
- Separate expressions to add region constraints.
- `a in b`, `a @ b`, or some such.

## To Do

- Name resolution.
  - Look down through type aliases via union and intersection types.
  - Look down through type parameters. Need an upper bounds.
- Assign: LHS expressions.
- A-normal form.
  - Need destructing assignment first.
- Auto create, default argument sugar, member conflict.
- Do we need:
  - Functions for field access?
  - LHS functions, with auto RHS versions?
- Undecided: type assertion, compile time evaluation, tuple flattening.
- Since loops are expression, should `break` and `continue` have values?
- Implement primitive types in `std::builtin`.
- `where` clause instead of `T1: T2 = T3`?
  - Then a value parameter can be `ident: type`.
- Code reuse.
- Structural types.
- FFI.
- Partial application, `_`.
- Pattern matching.
- Compile-time evaluation.
- Generators with `yield`.

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
       | qname // class, alias, or function
       | ident
       | expr '.' (ident | symbol) typeargs? // method
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
