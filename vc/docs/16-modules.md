# 16. Modules and Imports

[← Table of Contents](README.md) | [Previous: Concurrency](15-concurrency.md) | [Next: FFI →](17-ffi.md)

This chapter covers the module system, imports, and name resolution.

---

## 16.1 Module Structure

Each `.v` file defines a module named after the file (without the extension). A directory of `.v` files is treated as an implicit class scope:

```
my_project/
  main.v           → module "main"
  math.v           → module "math"
  utils/
    helpers.v      → module "utils::helpers"
```

All top-level declarations in a file are members of that module.

---

## 16.2 Qualified Access

Declarations in another module are accessed with `::`:

```verona
math::add(3, 4)
math::Vector
```

`::` works with any number of path segments:

```verona
utils::helpers::format(data)
```

---

## 16.3 `use` for Imports

The `use` declaration imports a module's contents for unqualified lookup within the current scope:

```verona
use math

// Now math's declarations are available unqualified:
add(3, 4)
```

### Scope

`use` affects only the scope it appears in. It does **not** affect qualified lookdown from outside — other modules still need to qualify `math::add()` even if the current module has `use math`.

### Multiple Imports

```verona
use math
use utils
```

---

## 16.4 `_builtin`

The `_builtin` module is implicitly available in every source file. It defines all primitive types (`i32`, `string`, `bool`, etc.) and fundamental operations.

You do not need to write `use _builtin` — it is automatically imported by the compiler.

---

## 16.5 Name Resolution Rules

Name resolution follows these rules:

1. **Local variables**: Checked first — `let` and `var` bindings in the current scope.
2. **Class/type lookup**: Walk up enclosing scopes looking for classes, type aliases, shapes.
3. **`use` imports**: Followed by `lookup()` — names from imported modules.
4. **Free functions**: NOT resolved by walking up scopes. Must be qualified or imported via `use`.

The rule that free functions are not resolved by scope walking is intentional — it prevents a free function (like `!=`) in an outer scope from accidentally shadowing a method of the same name on a type. See [Functions](07-functions.md).

---

## 16.6 Declaration Order

Declaration order within a file does not matter. The compiler resolves names across the entire module before processing:

```verona
// This works — main references cell before it's defined
main(): i32
{
  let c = cell(42);
  c.f
}

cell
{
  f: i32;
}
```
