# 23. Grammar Summary

[← Table of Contents](README.md) | [Previous: Built-in Types](22-builtin-types.md) | [Next: Error Handling →](24-error-handling.md)

Compact reference grammar for the Verona language. This is a readable summary, not a formal specification.

---

## 23.1 Program

```
Program       ::= Declaration*

Declaration   ::= ClassDef
                 | ShapeDef
                 | FunctionDef
                 | UseDecl
                 | FFIDecl
```

---

## 23.2 Classes and Shapes

```
ClassDef      ::= Ident TypeParams? '{' ClassBody '}'
ShapeDef      ::= 'shape' Ident TypeParams? '{' ShapeBody '}'

ClassBody     ::= (FieldDef | FunctionDef | ClassDef | ShapeDef | UseDecl)*
ShapeBody     ::= FunctionProto*

FieldDef      ::= Ident ':' Type ';'

TypeParams    ::= '[' Ident (',' Ident)* ']'
```

---

## 23.3 Functions

```
FunctionDef   ::= HandPrefix? Ident TypeParams? '(' Params? ')' WhereClause? (':' Type)? '{' Body '}'
FunctionProto ::= HandPrefix? Ident TypeParams? '(' Params? ')' WhereClause? (':' Type)? ';'

HandPrefix    ::= 'ref'

Params        ::= Param (',' Param)*
Param         ::= Ident ':' Type ('=' Expr)?
```

---

## 23.4 Types

```
Type          ::= TypeName
                 | TypeName TypeArgs
                 | Type '|' Type            // union
                 | Type '&' Type            // intersection
                 | Type '->' Type           // function (right-assoc)
                 | '(' TypeList ')'         // tuple
                 | '(' ')'                  // no-arg (for -> )
                 | 'self'                   // in shapes only

TypeName      ::= Ident ('::' Ident)*
TypeArgs      ::= '[' Type (',' Type)* ']'
TypeList      ::= Type (',' Type)*

WhereClause   ::= 'where' WhereExpr
WhereExpr     ::= Type '<' Type
                 | WhereExpr '&' WhereExpr
                 | WhereExpr '|' WhereExpr
                 | '!' WhereExpr
```

---

## 23.5 Expressions

```
Expr          ::= Literal
                 | Ident
                 | Expr '.' Ident                    // field / method access
                 | Expr '.' Ident '(' Args ')'       // method call
                 | Expr '(' Args ')'                  // juxtaposition (apply)
                 | TypeName '(' Args ')'              // constructor sugar
                 | TypeName '::' Ident '(' Args ')'   // qualified call
                 | Expr BinOp Expr                    // infix operator
                 | PrefixOp Expr                      // prefix operator
                 | ':::' Ident TypeArgs? '(' Args ')' // builtin / FFI
                 | 'new' '{' FieldInit (',' FieldInit)* '}'
                 | '::(' Args ')'                     // array literal
                 | '(' Args ')'                       // tuple / grouping
                 | 'if' Expr '{' Body '}' ElseChain?
                 | 'while' Expr '{' Body '}'
                 | 'for' Expr Ident '->' '{' Body '}'
                 | 'for' Expr '(' DestrElem (',' DestrElem)* ')' '->' '{' Body '}'
                 | 'when' '(' Args ')' '(' Params ')' '->' '{' Body '}'
                 | MatchExpr
                 | LambdaExpr
                 | 'return' Expr
                 | 'raise' Expr
                 | 'break'
                 | 'continue'
                 | 'ref' Expr
                 | '#' Expr

LambdaExpr    ::= '(' Params ')' (':' Type)? '->' LambdaBody
                 | Ident '->' LambdaBody
                 | '{' Body '}'

LambdaBody    ::= '{' Body '}'
                 | Expr

MatchExpr     ::= 'match' Expr '{' MatchArm (';' MatchArm)* '}' ElseChain?

MatchArm      ::= '(' Ident ':' Type ')' '->' LambdaBody    // type test
                 | '(' Expr ')' '->' LambdaBody              // value test

ElseChain     ::= 'else' '{' Body '}'
                 | 'else' 'if' Expr '{' Body '}' ElseChain?
                 | 'else' '(' Expr ')'

FieldInit     ::= Ident '=' Expr
                 | Ident                              // shorthand: ident = ident

Args          ::= Expr (',' Expr)*
                 | /* empty */

Body          ::= (Statement ';')* Expr
Statement     ::= Expr | LetDecl | VarDecl
```

---

## 23.6 Declarations (in Bodies)

```
LetDecl       ::= 'let' Ident (':' Type)? ('=' Expr)?
VarDecl       ::= 'var' Ident (':' Type)? ('=' Expr)?
Discard       ::= '_' '=' Expr
TupleDestr    ::= '(' DestrElem (',' DestrElem)* ')' '=' Expr
DestrElem     ::= LetDecl | VarDecl | Ident | '_' | SplatLet | SplatDC
SplatLet      ::= 'let' Ident '...'
SplatDC       ::= '_' '...'
```

---

## 23.7 Imports

```
UseDecl       ::= 'use' Ident                        // import module
                 | 'use' Ident '=' Type               // type alias
                 | 'use' '{' FFIFuncDecl* '}'         // FFI (no lib)
                 | 'use' String '{' FFIFuncDecl* '}'  // FFI (with lib)

FFIFuncDecl   ::= Ident '=' String '(' TypeList? (',' '...')? ')' ':' Type ';'
                 | 'init' '(' ')' ':' Type Block              // library init (inline body)
```

---

## 23.8 Literals

```
Literal       ::= IntLit | FloatLit | StringLit | CharLit | BoolLit | TypedLit

IntLit        ::= [0-9][0-9_]*
                 | '0b' [01_]+
                 | '0o' [0-7_]+
                 | '0x' [0-9a-fA-F_]+
FloatLit      ::= [0-9][0-9_]* '.' [0-9]+ ('e' [+-]? [0-9]+)?
                 | '0x' [0-9a-fA-F_]+ '.' [0-9a-fA-F]+ ('p' [+-]? [0-9]+)?
StringLit     ::= '"' ... '"'                         // escaped
                 | "'" '"' ... '"' "'"                 // raw (matching quotes)
CharLit       ::= "'" char "'"
BoolLit       ::= 'true' | 'false'
TypedLit      ::= TypeName Literal                    // e.g., i32 42
```

---

## 23.9 Operators

### Operator Characters

```
! # $ % & * + - / < = > ? @ \ ^ ` | ~
```

Combinations of these characters form operator symbols (e.g., `+`, `!=`, `<=`, `**`).

### Reserved Characters (not usable in operators)

```
' ( ) , . : ; [ ] _ { }
```

### Precedence (binding strength, tightest first)

1. `.` (dot access)
2. Juxtaposition / `:::` (application, builtins)
3. Infix/prefix operators (`+`, `-`, `*`, `<`, `==`, etc.)
4. `=` (assignment, right-associative)

> **All infix operators have the same precedence and are left-associative.** There is no precedence difference between `+` and `*`. Use parentheses for explicit grouping:

```verona
a + b * c                             // parsed as (a + b) * c — NOT a + (b * c)
a + (b * c)                           // use parens for multiplication-first
```

See [Chapter 5](05-expressions.md) for a full discussion of why this design was chosen.
