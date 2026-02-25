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
  new { x = x, y = y }
}
```

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

Writing requires a writable reference — the compiler handles this through `ref` accessors on fields.

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

## 8.6 Classes as Modules

Because each `.v` file is a class scope and each directory is an implicit class, classes serve as the module system. Top-level declarations in `utils.v` are accessed as `utils::name`. See [Modules and Imports](16-modules.md).
