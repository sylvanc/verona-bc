# Verona in 15 Minutes — A Four-Program Tour

Verona is a research language focused on concurrent ownership and memory safety. Programs compile to bytecode (`.vbc`) and run on an interpreter. There is no `println` — we verify behaviour through exit codes (0 = success).

**Build & run any example:**
```bash
cd build
dist/vc/vc build ../examples/v/tutorial1_basics
dist/vbci/vbci tutorial1_basics.vbc
echo $?   # 0
```

---

## Program 1 — Basics (~3 min)

**Key ideas:** entry point, `let`/`var`, while loops, if-expressions, functions, flat operator precedence.

```verona
// Every program needs main() returning i32 (the exit code).

abs(x: i32): i32
{
  if x < 0 { -x } else { x }
}

main(): i32
{
  var sum: i32 = 0;

  var i: i32 = 1;
  while i <= 10
  {
    sum = sum + i;
    i = i + 1
  }
  // sum = 55

  // if/else is an expression — returns a value
  let label = if sum > 50 { 1 } else { 0 };

  // Calling a free function (must be qualified)
  let d = tutorial1_basics::abs(-3);

  // ⚠ ALL operators have equal precedence!
  // a + b * c  →  (a + b) * c   — always use parens
  let check = d + (label * 0);

  var result = 0;
  if sum != 55 { result = result + 1 }
  if label != 1 { result = result + 2 }
  if check != 3 { result = result + 4 }
  result   // 0 = all passed
}
```

**Talking points:**
- Last expression in a block is the return value (no `return` needed).
- `let` = single-assignment, `var` = reassignable.
- `if`/`else` is an expression, not a statement.
- **Flat precedence** — `+`, `*`, `<` all bind the same. Always parenthesise.
- Free functions require qualified calls (`module::func`).

---

## Program 2 — Classes & Objects (~4 min)

**Key ideas:** no `class` keyword, auto-generated constructors, methods, operator overloading, field mutation through `let`.

```verona
// Classes use bare names — no "class" keyword
point
{
  x: i32;
  y: i32;

  manhattan(self: point): i32
  {
    abs(self.x) + abs(self.y)
  }

  // Operators are just methods
  +(self: point, other: point): point
  {
    point(self.x + other.x, self.y + other.y)
  }

  ==(self: point, other: point): bool
  {
    (self.x == other.x) & (self.y == other.y)
  }
}

abs(x: i32): i32 { if x < 0 { -x } else { x } }

// Default arguments
cell
{
  value: i32;
  create(value: i32 = 0): cell { new { value = value } }
}

main(): i32
{
  let a = point(3, 4);       // sugar for point::create(3, 4)
  let b = point(1, 2);

  let d = a.manhattan;       // zero-extra-arg method: no parens
  let c = a + b;             // calls +(self, other)

  // let constrains the binding, not the object
  let p = point(0, 0);
  p.x = 10;                  // field mutation is fine

  let empty = cell;           // default arg → cell::create(0)
  let full = cell(42);

  var result = 0;
  if d != 7 { result = result + 1 }
  if !(c == point(4, 6)) { result = result + 2 }
  if p.x != 10 { result = result + 4 }
  if empty.value != 0 { result = result + 8 }
  if full.value != 42 { result = result + 16 }
  result
}
```

**Talking points:**
- No `class` keyword — just `name { fields; methods; }`.
- Auto-generated `create` from fields (or write your own).
- `Type(args)` is sugar for `Type::create(args)`.
- `let` ≠ immutable object — `p.x = 10` works because `let` constrains the *binding*.
- Operators (`+`, `==`) are regular methods you define on your class.
- `new { field = value }` constructs inside a class body.

---

## Program 3 — Generics, Shapes & Arrays (~4 min)

**Key ideas:** shapes (structural interfaces), no `implements`, generic classes, arrays, type inference.

```verona
// Shape = structural interface
shape scorable
{
  score(self: self): i32;    // self = "whatever type satisfies this"
}

// These classes satisfy "scorable" — no declaration needed
task
{
  priority: i32;
  create(priority: i32 = 0): task { new { priority = priority } }
  score(self: task): i32 { self.priority }
}

bonus
{
  base: i32;
  multiplier: i32;
  create(base: i32 = 0, multiplier: i32 = 0): bonus
  { new { base = base, multiplier = multiplier } }
  score(self: bonus): i32 { self.base * self.multiplier }
}

// Takes a shape directly — any conforming class works
get_score(item: scorable): i32 { item.score }

// Generic wrapper — monomorphized at compile time
box[T]
{
  val: T;
  create(val: T): box[T] { new { val = val } }
  get(self: box[T]): T { self.val }
}

main(): i32
{
  var result = 0;

  // Shapes: structural subtyping
  let t1 = task(10);
  let b1 = bonus(3, 4);
  if tutorial3_generics::get_score(t1) != 10 { result = result + 1 }
  if tutorial3_generics::get_score(b1) != 12 { result = result + 2 }

  // Generics: type-safe containers
  let box_int = box[i32](42);
  if box_int.get != 42 { result = result + 4 }

  // Arrays: fixed-size, generic, bounds-checked
  let arr = array[i32]::fill(5);
  var i = 0;
  while i < arr.size
  {
    arr(i) = i.i32 + 1;     // .i32 converts usize → i32
    i = i + 1
  }

  var sum: i32 = 0;
  var j = 0;
  while j < arr.size { sum = sum + arr(j); j = j + 1 }
  if sum != 15 { result = result + 16 }

  // Array literals
  let nums = ::(i32 10, 20, 30);
  if nums(usize 1) != 20 { result = result + 32 }

  result
}
```

**Talking points:**
- **Shapes** are structural — `task` satisfies `scorable` because it has `score(self): i32`. No `implements`.
- `self: self` in a shape means "the concrete type that satisfies this shape."
- **Generics** (`box[T]`) are monomorphized — `box[i32]` and `box[bool]` produce separate code. No runtime dispatch.
- **Arrays** are fixed-size: `array[T]::fill(n)`. Indexing is `arr(i)` (juxtaposition calls `apply`).
- Array literals: `::(i32 10, 20, 30)` — first element's type annotation infers the rest.

---

## Program 4 — Lambdas & Higher-Order Functions (~4 min)

**Key ideas:** lambda syntax, closures, function types as shapes, stateful closures, generic higher-order functions.

```verona
// Higher-order function — f is a function type (shape with apply)
apply_twice(f: i32 -> i32, x: i32): i32
{
  f(f(x))
}

// Returns a stateful closure
make_counter(): () -> i32
{
  let count: i32 = 0;
  (): i32 -> { count = count + 1; count }
}

// Generic higher-order function
map_array[T](arr: array[T], f: T -> T): array[T]
{
  let result = array[T]::fill(arr.size);
  var i = 0;
  while i < arr.size
  {
    result(i) = f(arr(i));
    i = i + 1
  }
  result
}

main(): i32
{
  var result = 0;

  // Lambda expression
  let double = (x: i32): i32 -> { x + x };
  if double(5) != 10 { result = result + 1 }

  // Passing lambdas to higher-order functions
  let r = tutorial4_lambdas::apply_twice(double, 3);
  if r != 12 { result = result + 2 }    // double(double(3)) = 12

  // Stateful closure — captures mutable state
  let counter = tutorial4_lambdas::make_counter();
  let c1 = counter();
  let c2 = counter();
  let c3 = counter();
  if (c1 != 1) | (c2 != 2) | (c3 != 3) { result = result + 4 }

  // Transform an array with a lambda
  let nums = ::(i32 1, 2, 3, 4, 5);
  let squared = tutorial4_lambdas::map_array(
    nums,
    (x: i32): i32 -> { x * x }
  );

  // 1 + 4 + 9 + 16 + 25 = 55
  var sum: i32 = 0;
  var i = 0;
  while i < squared.size { sum = sum + squared(i); i = i + 1 }
  if sum != 55 { result = result + 8 }

  result
}
```

**Talking points:**
- Lambda syntax: `(params): RetType -> { body }`.
- **Function types are shapes** — `i32 -> i32` desugars to `shape { apply(self, i32): i32 }`.
- Lambdas desugar to anonymous classes with an `apply` method and captured variables as fields.
- Closures capture by reference — `make_counter` returns a closure that mutates `count` across calls.
- Generics + function types compose naturally: `map_array[T](arr, f: T -> T)`.

---

## Quick Reference Card

| Feature | Verona | Notes |
|---------|--------|-------|
| Class | `point { x: i32; y: i32; }` | No `class` keyword |
| Constructor | `point(3, 4)` | Sugar for `point::create(3, 4)` |
| Method | `self.manhattan` | First param is `self: ClassName` |
| Shape | `shape scorable { score(self: self): i32; }` | Structural interface |
| Generic | `box[T] { val: T; }` | Monomorphized |
| Array | `array[i32]::fill(10)` | Fixed-size, bounds-checked |
| Array literal | `::(i32 1, 2, 3)` | Sibling type inference |
| Lambda | `(x: i32): i32 -> { x + 1 }` | Desugars to class with `apply` |
| Function type | `i32 -> i32` | Desugars to shape |
| let/var | `let x = 1; var y = 2;` | Binding immutability, not object |
| Operators | All same precedence | **Always use parens** |
| Field access | `obj.field` / `obj.field = val` | Via auto-generated `ref` accessor |
| Return | Last expression in block | `return` for early exit |
