# Syntax Experiment

Infer the location for everything.
- Separate expressions to add region constraints.
- `a in b`, `a @ b`, or some such.

## To Do

- Let, var, assign, else, type assertion, compile time evaluation, tuple flattening, when, ref, try.
- Implement primitive types in `std::builtin`.
- `where` clause instead of `T1: T2 = T3`?
- Code reuse for classes.
- Structural types.
- FFI.
- Partial application, `_`.
- Pattern matching.
- Compile-time evaluation.
- Generators with `yield`.

## Syntax

```rs

keywords = 'use' | 'if' | 'else' | 'while' | 'for' | 'break' | 'continue'
         | 'return' | 'raise' | 'throw'
non-symbols = '=' | '#' | ':' | ';' | ',' | '.' | '(' | ')' | '[' | ']' | '{'
            | '}'

ptype = none | bool | i8 | i16 | i32 | i64 | u8 | u16 | u32 | u64 | f32 | f64
      | ilong | ulong | isize | usize
typename = ident typeargs? ('::' ident typeargs?)*
typearg = type | '#' expr
typeargs = '[' typearg (',' typearg)* ']'

// No precedence, read left to right.
type = ptype
     | typename
     | type '|' type // Union.
     | type '&' type // Intersection.
     | type ',' type // Tuple.
     | type '->' type // Function. Right associative.
     | '(' type ')' // Parentheses for grouping.

typeparam = ident (':' type)? ('=' type)?
typeparams = '[' (typeparam (',' typeparam)*)? ']'

top = class*

// Functions on primitive types.
primitive = ptype '{' function* '}'

// Classes.
class = ident typeparams? '{'
  (primitive | class | import | typealias | field | function)*
  '}'

// Imports.
import = 'use' typename

// Type alias.
typealias = 'use' ident typeparams? '=' type

param = ident (':' type)? ('=' expr)?
params = '(' param (',' param)* ')'
field = param
function = ident typeparams? params (':' type)? '{' body '}'
lambda = typeparams? params (':' type)? '->' (expr | '{' body '}')
       | '{' body '}'
tuple = expr (',' expr)*
body =
     ( import | typealias | 'break' | 'continue' | 'return' expr
     | 'raise' expr | 'throw' expr | expr
     )*

qname = typename '::' (ident | symbol) typeargs?
      | ident typeargs

// Expression binding.
strong = '(' expr ')' // exprseq
       | literal
       | tuple
       | lambda
       | qname
       | ident
       | expr '.' (ident | symbol) typeargs? // method
       | qname exprseq // static call
       | method exprseq // dynamic call
       | 'if' expr lambda
       | 'while' expr lambda
       | 'for' expr lambda
medium = apply expr // extend apply
       | expr expr // apply
weak   = symbol typeargs? expr // dynamic call
       | expr symbol typeargs? expr // dynamic call
       | expr 'else' expr
       | expr '=' expr
       | apply
         // qname: static call
         // method: dynamic call
         // other: dynamic call of `apply`

```
