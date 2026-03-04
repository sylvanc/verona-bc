# 8. Classes and Objects

[← Table of Contents](README.md) | [Previous: Functions](07-functions.md) | [Next: Shapes →](09-shapes.md)

This chapter covers class definitions, object construction, fields, and related patterns.

---

## 8.1 Class Definitions

Classes are declared with a bare name — no `class` keyword:

```verona
point
{
  x: i32;
  y: i32;
}
```

Fields are declared as `name: Type;` with a trailing semicolon.

### Generic Classes

Type parameters go in brackets after the class name:

```verona
wrapper[T]
{
  val: T;
}
```

### Methods Inside Classes

Functions defined inside a class body whose first parameter is `self: ClassName` are methods:

```verona
counter
{
  count: usize;

  increment(self: counter)
  {
    self.count = self.count + 1
  }

  get(self: counter): usize
  {
    self.count
  }
}
```

Functions without `self` are free functions scoped to the class. They must be called with a qualified name (e.g., `counter::create()`).

---

## 8.2 Object Construction

Objects are heap-allocated with the `new` keyword:

```verona
create(x: i32, y: i32): point
{
  new {x, y}
}
```

### Where Can `new` Be Used?

`new` creates an instance of the **enclosing class**. It can be used inside any function defined within the class body — not just `create`:

```verona
cell
{
  f: i32;

  make(x: i32): cell
  {
    new { f = x }                   // works — inside cell's body
  }
}
```

`new` cannot be used outside a class body. At the top level or inside `main()`, use constructor sugar instead: `cell(42)`.

### Field Shorthand

When the variable name matches the field name, the `= value` can be omitted:

```verona
create(val: T): wrapper[T]
{
  new { val }                 // equivalent to: new { val = val }
}
```

### Constructor Sugar

Placing arguments next to a type name calls `create`:

```verona
let p = point(1, 2);     // sugar for point::create(1, 2)
let w = wrapper[i32](42);        // sugar for wrapper[i32]::create(42)
```

If no `create` function is defined, one is auto-generated from the field definitions. See [Functions](07-functions.md).

---

## 8.3 Field Access

Fields are read with dot syntax:

```verona
let x = p.x;                 // read field x
let v = w.val;                // read field val
```

Fields are written with dot-assignment:

```verona
p.x = 5;                 // write field x
```

### How Field Assignment Works

When you write `p.x = 5`, the compiler:

1. Calls the compiler-generated `ref x(self: point): ref[i32]` accessor, producing a `ref[i32]` pointing into the object.
2. Emits a `Store` instruction that writes `5` through the reference.
3. The store performs a region-aware exchange — checking ownership rules, updating reference counts, and setting parent pointers if the stored value comes from a different region.
4. The old value at that field is returned (assignment is an exchange — see [Expressions §5.13](05-expressions.md)).

You don't need to think about these steps — `p.x = 5` just works. But understanding the mechanism explains why field writes can trigger region dragging (see [Memory Model §19.3](19-memory-model.md)).

---

## 8.4 Nested Classes

Classes can be nested. Inner classes are scoped to the outer class:

```verona
outer
{
  inner
  {
    val: i32;
  }
}
```

Access the inner class with a qualified name: `outer::inner`.

---

## 8.5 Shadowing

A nested class can shadow an outer class of the same name. Resolution uses the innermost visible definition:

```verona
wrapper
{
  val: i32;

  // An inner "wrapper" shadows the outer one within this scope
  wrapper
  {
    val: string;
  }
}
```

---

## 8.6 Visibility

All declarations in Verona are **public**. There are no access modifiers (`private`, `protected`, `internal`, etc.):

- All fields are readable and writable from any scope.
- All methods are callable from any scope.
- All classes and shapes are visible from any scope.
- Nested classes are accessible via qualified names (e.g., `outer::inner`).

Encapsulation and access control are still being designed. The current all-public model may change in future versions of the language.

---

## 8.7 No Inheritance

Verona has **no class inheritance** — no `extends`, no `super`, no override. Polymorphism is achieved through [shapes](09-shapes.md) (structural subtyping):

- Instead of an abstract base class, define a **shape** with the required method signatures.
- Instead of inheriting method implementations, each class provides its own.
- Instead of `Animal → Dog → GoldenRetriever`, you'd define shapes like `named`, `trainable`, `pet` and have each class satisfy whichever shapes apply.

This is a deliberate design choice. Structural subtyping avoids the fragile base class problem, doesn't require declaration-site `implements` annotations, and composes better with generics and monomorphization.

If you need shared code between types that satisfy the same shape, put the shared logic in a free function that takes the shape as a parameter:

```verona
shape valued
{
  val(self: self): i32;
}

// Shared logic for any type satisfying `valued`
double_val[T](v: T): i32 where T < valued
{
  v.val + v.val
}
```

---

## 8.8 Classes as Modules

Each directory is an implicit class scope (a module). Files within a directory contribute their declarations to the module — the file's name is not semantically significant. Because classes and modules are the same construct, top-level declarations in a project directory are accessed as `module::name`. See [Modules and Imports](16-modules.md).
