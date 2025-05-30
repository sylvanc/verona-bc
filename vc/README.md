# Syntax Experiment

Infer the location for everything.
- Separate expressions to add region constraints.
- `a in b`, `a @ b`, or some such.

## To Do

- Expression grouping.
- Else, var/let, when, ref, try.
- Assignment, LHS and RHS expressions.
- How does dot actually work?
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
      | symbol typeargs?

expr = ident // Local, function pointer, or create-sugar for a class.
     | qname expr // (apply qname expr)
       // want to be able to use an ident as well instead of a qname. but if the ident is a local, it should be an application.
       // have to do expr.ident instead, or use a symbol.
     | expr1 qname expr2 // (apply (lookup expr1 op) expr1 expr2)
       // want to be able to use an ident as well instead of a qname. but if the ident is a local, it should be an application.
       // have to do expr1.ident expr2 instead, or use a symbol.
       // if this is a class, we get Class::create(expr1, expr2)
     | expr '.' ident typeargs? // (apply (lookup expr ident typeargs) expr)
     | expr '.' symbol typeargs? // (apply (lookup expr symbol typeargs) expr)
     | expr1 expr2 // (apply expr1 expr2), extend if expr1 is `apply`
     | expr1 '.' expr2 // (apply expr2 expr1), extend if expr2 is `apply`
     | expr '=' expr // Assignment.
     // done:
     | '(' expr ')'
     | literal
     | tuple
     | lambda
     | qname // Function pointer, or create-sugar for a class.
     | 'if' expr lambda ('else' 'if' expr lambda)* ('else' lambda)?
     | 'while' expr lambda
     | 'for' expr lambda

```
