# 16. Modules and Imports

[← Table of Contents](README.md) | [Previous: Concurrency](15-concurrency.md) | [Next: FFI →](17-ffi.md)

This chapter covers the module system, imports, and name resolution.

---

## 16.1 Module Structure

A project directory is a module. Each `.v` file inside it defines declarations that become part of that module — the file name does not create a separate module scope. If a directory name contains `-`, the Verona module name normalizes it to `_`. Subdirectories create nested module scopes:

```
my_project/
  main.v           → declarations in the "my_project" module
  math.v           → declarations in the "my_project" module
  utils/
    helpers.v      → declarations in the "utils" nested module
```

All top-level declarations across `.v` files in the same directory share one scope.

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

---

## 16.7 Multi-File Project Example

Here is a complete example of a multi-file project demonstrating how modules interact:

### Project Structure

```
calculator/
  main.v
  math.v
  types.v
```

### `types.v`

```verona
result
{
  value: i32;
  ok: bool;
}
```

### `math.v`

```verona
use types

add(a: i32, b: i32): result
{
  result(a + b, true)
}

safe_div(a: i32, b: i32): result
{
  if b == 0
  {
    result(0, false)
  }
  else
  {
    result(a / b, true)
  }
}
```

### `main.v`

```verona
use math

main(): i32
{
  let sum = add(10, 20);
  let div = safe_div(sum.value, 3);

  if div.ok
  {
    div.value
  }
  else
  {
    99
  }
}
```

### Building and Running

```bash
cd build
dist/vc/vc build ../calculator
dist/vbci/vbci calculator.vbc
echo $?                               # prints 10
```

Key points:
- All `.v` files in a directory contribute to the same module scope.
- `use math` imports `math`'s declarations for unqualified use.
- Without `use`, you can still access declarations with qualified names: `math::add(10, 20)`.
- `use types` in `math.v` makes the `result` class available without qualifying it as `types::result`.

---

## 16.8 The Various Forms of `use`

`use` appears in several different contexts. Here they all are in one place:

| Form | Purpose | Example |
|------|---------|--------|
| `use Module` | Import a local class/module for unqualified lookup | `use math` |
| `use x = Module` | Import a local class/module with an alias | `use m = math` |
| `use "url" "tag" "dir"` | Import a package from a git URL at a specific git ref (the subdirectory is optional) | `use "https://..." "v1.0"` |
| `use x = "url" "tag" "dir"` | Named alias at a specific git ref | `use mylib = "https://..." "v1.0"` |
| `use "library" { name = "lib"(...): T; }` | FFI declarations (see [FFI §17](17-ffi.md)) | `use { print = "print"(any): none; }` |

### The `ffi` Namespace

The `_builtin/ffi/` directory defines wrapper functions for common FFI operations. Because `_builtin` is always imported and `ffi/` is a nested scope, these functions are accessed via `ffi::`:

```verona
ffi::external.add;                    // call add_external from _builtin/ffi/notify.v
let cb = ffi::callback(my_lambda);    // callback from _builtin/ffi/callback.v
```

See [FFI §17.8](17-ffi.md) for the full list of `ffi::` functions.

### `use Module` — Local Import

Imports a class or module's declarations for unqualified lookup in the current scope:

```verona
use math
add(3, 4)                             // resolves to math::add
```

This affects only `lookup` (walking up scopes). It does **not** make the declarations visible via `lookdown` from outside — other modules still need `math::add()`.

### `use "url" "tag"` — Package Import

Imports an external package from a git repository URL:

```verona
use "https://github.com/user/repo" "tag"
```

The package's top-level declarations become available for unqualified lookup, just like `use Module`.

The second string specifies a **git ref** (branch, tag, or commit hash):

```verona
use "https://github.com/user/repo" "v1.0"      // specific tag
use "https://github.com/user/repo" "main"      // branch name
```

An optional third string specifies a **subdirectory** within the repository:

```verona
use "https://github.com/user/repo" "main" "src" // import the src/ subdirectory
```

The dependency system uses `git fetch`/`checkout`, so changes must be committed and pushed to the repo before the consuming project can see them.

For local development, you can use a local directory path, which skips git and copies directly from the file system:

```verona
use "~/dev/my-package"                   // local directory
```

This is useful for testing changes to a package without pushing to a remote repo. The path can be absolute or relative to the current project.

### `use x = "url" "tag"` — Named Package Alias

Imports a package and binds it to a name for qualified access:

```verona
use mylib = "https://github.com/user/repo" "v1.0"
mylib::some_function()
```

### `use { ... }` — FFI Declarations

Declares foreign function interfaces. See [FFI](17-ffi.md) for the full syntax:

```verona
use
{
  print = "print"(any): none;
}
```

The `use` keyword is overloaded, but the forms are syntactically distinct — the compiler disambiguates by what follows `use` (a name, a string, an assignment, or a brace block).
