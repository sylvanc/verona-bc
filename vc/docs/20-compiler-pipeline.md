# 20. Compiler Pipeline

[← Table of Contents](README.md) | [Previous: Memory Model](19-memory-model.md) | [Next: Toolchain Usage →](21-toolchain-usage.md)

This chapter describes the internal architecture of the `vc` compiler for contributors and AI agents working on the codebase.

---

## 20.1 Overview

The `vc` compiler is a multi-pass term rewriting compiler built on the [Trieste](https://github.com/microsoft/trieste) framework. Source code is parsed into an AST (abstract syntax tree), then transformed through a sequence of passes. Each pass rewrites the tree toward a simpler, more explicit form until it is ready for bytecode generation.

---

## 20.2 Pass Pipeline

The compiler runs passes in two stages. The first 10 passes are the `vc`
frontend, which transforms source code into monomorphized VIR. The remaining
passes are provided by VIRC, which creates output-neutral `Compilation` state
for either the VBC or LLVM emitter.

### Frontend Passes (vc)

| # | Pass | Direction | Purpose |
|---|------|-----------|---------|
| 0 | `parse` | — | Tokenization and initial AST construction |
| 1 | `structure` | top-down | Scope nesting, class/function/type structure, auto-create constructors |
| 2 | `ident` | bottom-up / once | Name resolution, fully qualified names, symbol tables |
| 3 | `sugar` | top-down | When rewriting, match desugaring, lambda desugaring, default args, auto-RHS |
| 4 | `functype` | bottom-up | Function type (`->`) → synthetic shape conversion |
| 5 | `dot` | top-down | Dot access, juxtaposition (application), `:::` builtin/FFI resolution |
| 6 | `application` | top-down | Infix/prefix function/method calls, ref, hash, partial application |
| 7 | `anf` | top-down | A-Normal Form: flatten expressions to SSA-like three-address statements |
| 8 | `infer` | once | Type inference and literal refinement |
| 9 | `reify` | bottom-up | Monomorphization — generic instantiation starting from `main` |

### VIRC Passes

| # | Pass | Direction | Purpose |
|---|------|-----------|---------|
| 10 | `memo` | once | Split `once` functions into stub + init, topological sort, cycle detection |
| 11 | `assignids` | once | Assign stable identifiers to classes, functions, methods |
| 12 | `validids` | once | Validate identifier assignments for consistency |
| 13 | `typecheck` | once | Final type checking and output-neutral type state |
| 14 | `optimize` | once | Shared VIR optimization |
| 15 | `liveness` | once | Liveness analysis and explicit drops |

After all passes complete, the selected peer emitter produces VBC or LLVM IR.
In practice, `vc build` invokes both stages; the user does not run VIRC
separately.

---

## 20.3 AST Representation

The AST is a Trieste node tree. Each node has:
- A **token type** (e.g., `ClassDef`, `Function`, `Call`, `Const`)
- Zero or more **children** (other nodes)
- An optional **location** (source position) and **value** (string data)

Children are accessed by named token type using `node / ChildToken`:

```cpp
auto body = func / Body;           // get the Body child of func
auto name = cls / Ident;           // get the Ident child of cls
```

---

## 20.4 Well-Formedness (WF)

Each pass declares a WF specification describing the expected AST shape of its output. The WF uses operators:
- `<<=` — child structure
- `++` — zero or more
- `|` — alternatives
- `[Include]` — symtab include entry
- `[Ident]` — lookup by Ident child

If the AST violates WF after a pass, processing halts with an error. WF acts as a contract between passes.

---

## 20.5 Error Handling

Errors are AST nodes, not exceptions:

```cpp
Error << (ErrorMsg ^ "message") << (ErrorAst << problematic_node)
```

This wraps the erroneous subtree in an `Error` node, exempting it from WF checks so the pass can continue processing the rest of the tree. Errors are collected and reported at the end.

---

## 20.6 Key Passes in Detail

### Parse (`parse`)
Tokenizes source code using regex-based rules. Produces a flat token stream grouped by `()`, `[]`, `{}`. Handles comments, string literals, numeric literals, operators, keywords, and identifiers.

### Structure (`structure`)
Converts the flat token groups into nested AST structure: class definitions, function definitions, type expressions, control flow nodes, expressions. Enforces structural rules (e.g., shapes can't have function bodies, prototypes only in shapes). In its `post()` hook, generates default `create` constructors for classes that don't define one.

### Ident (`ident`)
Resolves all names to fully qualified paths from the top scope. Uses `NodeWorker<Resolver>` for concurrent resolution with blocking dependencies. Builds the symbol table and resolves `use` imports.

### Sugar (`sugar`)
Desugars syntactic sugar:
- `match` expressions → case lambda chains with `nomatch` subtraction and `TryCallDyn` for value tests
- Lambdas → anonymous classes with `apply`
- `when` blocks → validated cown access
- Default arguments → wrapper functions
- Auto-RHS → generated value accessors for `ref` functions

### Dot (`dot`)
Resolves dot access (`a.b`), juxtaposition (`a(b)` → `a.apply(b)`), type constructor calls (`Type(args)` → `Type::create(args)`), and `:::` builtin/FFI calls.

### Application (`application`)
Resolves infix/prefix operators to method calls, handles `ref` and `#` prefixes, and desugars partial application (`_` placeholders) to anonymous classes.

### ANF (`anf`)
Converts the AST to A-Normal Form: all intermediate values are named, expressions are flattened to sequences of simple statements. This prepares the code for type inference and bytecode generation.

### Infer (`infer`)
Type inference pass (`dir::once`). Builds a type environment mapping variables to types, then refines default-typed literals (u64/f64) based on context. Handles call argument types, variable annotations, field types, return types, FFI types, shape matching, backward refinement, and cascade propagation.

### Reify (`reify`)
Monomorphization pass (`dir::once`). Starting from `main()`, transitively instantiates all reachable generic classes and functions. Each unique type argument combination produces a separate specialization. Shapes are not monomorphized — they use dynamic dispatch directly. Outputs IR suitable for bytecode generation.

### Typecheck (`typecheck`)
The final backend type checker rejects errors that are determined by the
reified program rather than by runtime state. These include incompatible
primitive conversions, operand and argument mismatches, statically missing
methods, and using an empty (singleton) class as a new region's entry point.
The bytecode interpreter retains checks for these cases, but LLVM output does
not rely on runtime validation for an operation the compiler can prove
invalid.

---

## 20.7 Debugging Passes

Stop after a specific pass:
```bash
vc build project/ -p anf          # stop after ANF pass
```

Dump intermediate ASTs:
```bash
vc build project/ --dump_passes=./dump/
```

This creates one `.trieste` file per pass in the dump directory, letting you inspect the AST at each stage.

---

## 20.8 Standalone VIR Compiler (virc)

The `virc` tool can also be run standalone on textual VIR files (produced by
`vc` with `-p reify`). It prepends two reader passes before the shared VIRC
pipeline:

| # | Pass | Purpose |
|---|------|---------|
| 0 | `statements` | Parse Trieste IR text into statement sequences |
| 1 | `labels` | Resolve jump targets and label offsets |
| 2–7 | (shared) | `memo` → `assignids` → `validids` → `typecheck` → `optimize` → `liveness` |

When `vc build` is used, these reader passes are not needed because VC passes
VIR directly to `virc_core`. The `vbcc` command remains as a migration alias.
