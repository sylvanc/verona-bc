# 5. Expressions

[← Table of Contents](README.md) | [Previous: Declarations](04-declarations.md) | [Next: Control Flow →](06-control-flow.md)

This chapter covers all expression forms in Verona.

---

## 5.1 Literals

### Numeric Literals

```verona
42                            // integer (default: u64, inferred from context)
3.14                          // float (default: f64, inferred from context)
0b1010                        // binary integer: 10
0o755                         // octal integer: 493
0xFF                          // hex integer: 255
1_000_000                     // underscores as digit separators
1.0e10                        // scientific notation
0x1.0p10                      // hex float
```

Unadorned integer literals default to `u64` and float literals to `f64`. The [type inference](18-type-inference.md) pass refines these based on context (function parameter types, variable annotations, field types).

To force a specific type, prefix the literal with the type name:

```verona
i32 42                        // i32 literal
f32 3.14                      // f32 literal
usize 0                       // usize literal
```

### Boolean Literals

```verona
true
false
```

### String Literals

```verona
"hello, world"                // escaped string
```

#### Escape Sequences

| Escape | Meaning |
|--------|--------|
| `\a` | Alert (bell) |
| `\b` | Backspace |
| `\f` | Form feed |
| `\n` | Newline |
| `\r` | Carriage return |
| `\t` | Tab |
| `\v` | Vertical tab |
| `\\` | Backslash |
| `\'` | Single quote |
| `\"` | Double quote |
| `\?` | Question mark |
| `\0`–`\7` | Octal (up to 3 digits) |
| `\xNN` | Hex (variable length) |
| `\uNNNN` | Unicode (exactly 4 hex digits) |
| `\UNNNNNNNN` | Unicode (exactly 8 hex digits) |

All escape sequences produce UTF-8. Regular `"..."` strings are single-line.

### Character Literals

```verona
'a'                           // character literal — returns u8
'\n'                          // escape sequences supported
```

### Raw Strings

Raw strings use matching quote delimiters to avoid escaping:

```verona
'"this has "quotes" inside"'
''"contains 'single quotes' too"''
```

The number of leading single-quotes must match the trailing ones. Escape sequences are NOT processed inside raw strings. Raw strings can span multiple lines.

---

## 5.2 Arithmetic Operators

All arithmetic operators are method calls on the receiver type:

| Operator | Meaning | Example |
|----------|---------|---------|
| `+` | Addition | `a + b` |
| `-` | Subtraction | `a - b` |
| `*` | Multiplication | `a * b` |
| `/` | Division | `a / b` |
| `%` | Modulo | `a % b` |
| `**` | Power (`f64` only) | `a ** b` |

Unary negation: `-a` (prefix `-`).

Since operators are methods, `a + b` is equivalent to calling the `+` method on `a` with argument `b`. This is how operator overloading works — define a method named `+` on your class. See [Functions](07-functions.md).

---

## 5.3 Bitwise Operators

| Operator | Meaning | Example |
|----------|---------|---------|
| `&` | Bitwise AND | `a & b` |
| `\|` | Bitwise OR | `a \| b` |
| `^` | Bitwise XOR | `a ^ b` |
| `<<` | Left shift | `a << b` |
| `>>` | Right shift | `a >> b` |
| `!` | Bitwise NOT (prefix) | `!a` |

These are defined on integer types. Note that `&` and `|` on `bool` have different (short-circuit) semantics — see [Logical Operators](#55-logical-operators).

---

## 5.4 Comparison Operators

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `<=` | Less than or equal |
| `>` | Greater than |
| `>=` | Greater than or equal |

All comparison operators return `bool`.

---

## 5.5 Logical Operators

On `bool`, `&` and `|` use **short-circuit evaluation**:

```verona
// & evaluates: if self is false, returns false without evaluating other
// | evaluates: if self is true, returns true without evaluating other
```

The right-hand side of `&` and `|` is a `to_bool` shape:

```verona
shape to_bool
{
  apply(self: self): bool;
}
```

This means the RHS is lazily evaluated — it is called only if needed. Any object with an `apply` method returning `bool` can appear as the RHS.

Boolean negation uses the `!` prefix operator:

```verona
!(self == other)              // logical NOT
```

---

## 5.6 Field Access and Method Calls

Dot syntax accesses fields and calls methods:

```verona
obj.field                     // read a field
obj.field = val               // write a field
obj.method(arg1, arg2)        // call a method with arguments
obj.method                    // call a zero-argument method (no parens needed)
```

Methods are functions whose first parameter is `self`. When called via dot syntax, the receiver becomes the `self` argument. See [Functions](07-functions.md).

### Qualified Calls and Dot Chaining

Qualified function calls can be chained with dot access:

```verona
module::func(args).method             // call func, then call method on the result
module::func.method                   // call zero-arg func, then call method on result
```

When a qualified name (`Ns::f`) is immediately followed by `.method`, the compiler auto-generates a zero-argument call to `f` and then applies the dot:

```verona
ffi::external.add                     // equivalent to: (ffi::external()).add
```

When a qualified name is followed by arguments *and* a dot, the arguments are consumed first:

```verona
module::func(x, y).result             // calls func(x, y), then .result on the return value
```

---

## 5.7 Indexing (Juxtaposition / Apply)

Juxtaposition — placing arguments next to a value — calls the `apply` method:

```verona
arr(i)                        // calls arr.apply(i) — read element
arr(i) = val                  // calls arr.ref apply(i), then stores — write element
```

This works on any type that defines `apply`. For arrays, `apply` returns the element at the given index. For mutable access, a `ref apply` method returns a `ref[T]`.

---

## 5.8 Binding Precedence

Operators bind in this order (tightest first):

1. **Dot access** (`.`) — tightest
2. **Juxtaposition** (function application) — `a(b)`
3. **Infix operators** — `+`, `-`, `*`, `<`, etc.
4. **Assignment** (`=`) — loosest, right-associative

This means:

```verona
sum + arr(i)                  // parsed as: sum + (arr.apply(i))
obj.field + 1                 // parsed as: (obj.field) + 1
```

Use parentheses to override:

```verona
(obj.field)(args)             // call apply on the result of a field access
```

Note: `obj.f(args)` calls method `f` with `args` — not `apply` on field `f`. To call `apply` on a field, use `(obj.f)(args)` or `obj.f.apply(args)`. See [Chapter 1](01-getting-started.md) for more.

> **⚠️ IMPORTANT: Flat operator precedence.** All infix operators have **the same precedence** and are **left-associative**. There is no precedence difference between `+` and `*`. This is the single most common source of confusion for new Verona programmers. It means:
>
> ```verona
> a + b * c                    // parsed as (a + b) * c — NOT a + (b * c)
> a * b + c                    // parsed as (a * b) + c
> a < b & c > d                // parsed as ((a < b) & c) > d
> ```
>
> **Always use parentheses** to get the intended grouping:
>
> ```verona
> a + (b * c)                  // multiplication first
> (a < b) & (c > d)            // comparisons first
> ```
>
> This is a deliberate design choice — it avoids implicit precedence surprises for user-defined operators. Since all operators are method calls, there is no reason `*` should bind tighter than a custom `%%` operator.
>
> See [Gotchas §26.1](26-gotchas.md) for more on this and other common pitfalls.

---

## 5.9 Constructor Calls

Placing arguments next to a type name calls the `create` constructor:

```verona
cell(7)                       // sugar for cell::create(7)
wrapper[i32](42)              // sugar for wrapper[i32]::create(42)
string(data)                  // sugar for string::create(data)
```

---

## 5.10 Identity and Raw Bits

Three free functions in `_builtin` operate on value identity:

```verona
is(a, b)                      // true if a and b are the same value
isnt(a, b)                    // true if a and b are different values
bits(a)                       // the bit representation of the value as u64
```

These take `any` — they work on any type. For objects and arrays, the value is the address of the allocation. For primitives, the value is the bit representation of the primitive (e.g., `bits(42)` returns `0x000000000000002A`, and `bits(0.5)` returns `0x3FE0000000000000`).

---

## 5.11 Dereference (`*`)

The `*` operator is defined as a `ref` method on `ref[T]`, returning a `ref[T]`. It allows both reading and writing through the reference.

For example, in a `when`, the parameters are each a `ref` to the cown contents. You can use `*` to dereferences a cown content reference to access the underlying value:

```verona
when (c) (x) ->
{
  (*x).field                          // access field of the cown's value
}
```

---

## 5.12 Return

`return` exits a function early with a value:

```verona
<(self: string, other: string): bool
{
  var i = 0;
  while i < self.size
  {
    if i == other.size { return false };
    if (self.data)(i) < (other.data)(i) { return true };
    if (self.data)(i) > (other.data)(i) { return false };
    i = i + 1
  }
  self.size < other.size
}
```

`return` is a statement — it cannot be used as a sub-expression. The last expression in a block is implicitly returned without the keyword.

---

## 5.13 Assignment

Assignment uses `=` and is right-associative:

```verona
x = 42;
obj.field = val;
arr(i) = val;
```

Assignment to a field requires a writable reference (a `ref` accessor). Assignment to an indexed position calls `ref apply`.

### Assignment is an Expression

Assignment evaluates to the **previous value** of the left-hand side:

```verona
var x = 1;
let old = x = 2;                   // old is 1, x is now 2
```

For field writes through `ref[T]`, the store also returns the old value. See [Memory Model](19-memory-model.md) for details.

---

## 5.14 Match Expressions

`match` tests a value against a sequence of type test and value test patterns:

```verona
let result = match x
{
  (n: i32) -> n + 1;
  (s: string) -> s.size;
}
else (0);
```

The `else` clause is optional — without it, a non-exhaustive match returns `nomatch`. Arms are tried in order — the first match wins.

- **Type test arms** `(x: T) -> body` bind the value if it matches the type.
- **Value test arms** `(expr) -> body` compare using `==`.

See [Control Flow §6.8](06-control-flow.md) for full syntax and examples.
