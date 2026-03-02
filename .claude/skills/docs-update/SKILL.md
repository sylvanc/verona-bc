---
name: docs-update
description: Update the Verona language documentation after adding a new feature. Use when the user asks to update docs, or after completing a feature implementation.
argument-hint: <feature-description>
---

# Update Verona Language Documentation

Update the documentation in `vc/docs/` to reflect a new feature or change to the Verona compiler.

## Arguments

- `$0` — a description of the feature or change that needs documenting (e.g., "callback support", "new builtin type", "new syntax")

## Documentation Structure

The docs live in `vc/docs/` with 27 numbered chapters plus a `README.md` table of contents:

| File | Content | When to update |
|------|---------|----------------|
| `README.md` | Table of contents | Only when adding new chapters |
| `01-getting-started.md` | First look, `_builtin` directory listing | New `_builtin` files or subdirectories |
| `02-program-structure.md` | Top-level structure, `_builtin` overview | New top-level constructs |
| `03-types.md` | Type system: primitives, unions, special | New primitive types or type syntax |
| `04-declarations.md` | `let`, `var`, `_`, scoping | New declaration forms |
| `05-expressions.md` | All expression forms, operators, precedence | New operators, expression syntax |
| `06-control-flow.md` | `if`, `while`, `for`, `match`, `raise`, `when` | New control flow constructs |
| `07-functions.md` | Functions, methods, operators, `ref`, overloading | New function features |
| `08-classes-and-objects.md` | Class definitions, fields, constructors | New class features |
| `09-shapes.md` | Structural types / interfaces | New shape features |
| `10-generics.md` | Type parameters, monomorphization, inference | Generics changes |
| `11-tuples.md` | Tuple construction, destructuring, splat | Tuple changes |
| `12-arrays.md` | Array creation, indexing, iteration, literals | Array changes |
| `13-lambdas.md` | Lambdas, closures, free variables | Lambda changes |
| `14-partial-application.md` | `_` placeholders, partial application | Partial application changes |
| `15-concurrency.md` | Cowns, `when` blocks, FAQ | Concurrency features, external resources |
| `16-modules.md` | `use`, imports, namespaces | New modules, namespaces, import forms |
| `17-ffi.md` | FFI: `use` blocks, builtins, callbacks, init | **Most FFI changes go here** |
| `18-type-inference.md` | Inference rules, refinement, backward refinement | Inference algorithm changes |
| `19-memory-model.md` | Regions, ownership, `ref[T]`, runtime errors | Memory model changes, new runtime errors |
| `20-compiler-pipeline.md` | Pass descriptions, AST, WF, debugging | New passes or pass changes |
| `21-toolchain-usage.md` | CLI flags, build commands | New CLI options |
| `22-builtin-types.md` | All `_builtin` types with method tables | **New builtin types go here** |
| `23-grammar-summary.md` | Formal grammar rules | **Any syntax changes go here** |
| `24-error-handling.md` | Error patterns, `raise`, `else` | Error handling changes |
| `25-compile-time-execution.md` | `#` operator (placeholder) | Compile-time execution features |
| `26-gotchas.md` | Common surprises and pitfalls | New pitfalls for the feature |
| `27-common-patterns.md` | Idiomatic patterns and recipes | **New usage patterns go here** |

## Steps

### 1. Identify which docs need updating

Read the feature description and determine which documentation files are affected. Use this checklist organized by feature type:

**New builtin type (e.g., `callback`, `ptr`):**
- `22-builtin-types.md` — Add type section with method table
- `01-getting-started.md` — Update `_builtin` directory listing if new file added
- `03-types.md` — If it's a new primitive or special type
- Relevant feature chapter (e.g., `17-ffi.md` for FFI-related types)

**New `_builtin` file or subdirectory:**
- `01-getting-started.md` — Update `_builtin` directory tree listing
- `02-program-structure.md` — If it changes the top-level structure
- `16-modules.md` — If it introduces a new namespace (e.g., `ffi::`)

**New syntax (keyword, operator, construct):**
- `23-grammar-summary.md` — Add or modify grammar production rules
- Relevant feature chapter for usage documentation
- `05-expressions.md` — If it's a new expression form
- `06-control-flow.md` — If it's a new control flow construct

**New FFI feature:**
- `17-ffi.md` — Primary documentation (add new section)
- `22-builtin-types.md` — If new builtin types/functions are exposed
- `17-ffi.md` table of builtin categories — Add rows for new builtins
- `15-concurrency.md` FAQ — If it affects scheduling/external resources
- `27-common-patterns.md` — Add usage pattern

**New compiler pass or pass change:**
- `20-compiler-pipeline.md` — Update pass table or descriptions

**New CLI option:**
- `21-toolchain-usage.md` — Add option documentation

### 2. Read the source of truth

Before writing documentation, read the actual implementation to ensure accuracy:

- **`_builtin` files**: `vc/_builtin/` — Verona source defining types and methods
- **FFI builtins**: `vc/_builtin/ffi/` — FFI wrapper functions
- **Builtin categories table**: `vc/passes/dot.cc` — The `builtins` map defines all `:::` builtins
- **Grammar**: `vc/passes/parse.cc` for tokenization, `vc/passes/structure.cc` for AST structure
- **WF definitions**: `vc/lang.h` for frontend WF, `include/vbcc.h` for backend WF
- **Interpreter behavior**: `vbci/thread.cc` for runtime semantics

### 3. Write the documentation

Follow these conventions:

**Section numbering:**
- Sections use `## N.M Title` format where N is the chapter number
- When adding sections at the end, use the next available number
- When inserting sections, renumber subsequent sections and update all internal cross-references

**Cross-references:**
- Format: `[Title §N.M](filename.md)` for section references
- Format: `[Title](filename.md)` for chapter references
- Chapter headers have navigation links: `[← Table of Contents](README.md) | [Previous: Title](prev.md) | [Next: Title →](next.md)`

**Code examples:**
- Use ` ```verona ` fenced blocks for Verona source code
- Use ` ```bash ` for shell commands
- Use ` ```cpp ` for C/C++ code (e.g., in FFI examples)
- Examples should be self-contained and runnable where possible
- Follow idiomatic Verona style (see CLAUDE.md "Idiomatic Verona style")

**Method/API tables:**
- Use `| Method | Signature | Description |` format
- Include all public methods for builtin types
- Group related methods logically

**Tone and style:**
- Direct and technical — written for programmers
- Explain "why" for non-obvious design decisions
- Use "See [Chapter](file.md)" for cross-references rather than repeating content
- Mark unimplemented/planned features with `> **Status:** Not yet implemented.`

### 4. Verify cross-references

After writing, check that:
- All `[text](file.md)` links point to files that exist
- Section numbers referenced in cross-links (`§N.M`) match actual section numbers
- No dangling references to renamed/renumbered sections
- The README.md table of contents is still accurate

### 5. Check for ripple effects

A documentation change in one file often requires updates in others:
- Adding a section to `17-ffi.md` may need a cross-reference from `22-builtin-types.md`
- New syntax in `23-grammar-summary.md` should be explained in the relevant feature chapter
- New patterns in `27-common-patterns.md` should cross-reference the feature chapter
- New gotchas in `26-gotchas.md` should cross-reference relevant chapters
- If you renumber sections, search all docs for `§old_number` references to update

### 6. Report

State which files were updated, what sections were added/modified, and any cross-references that were created or updated.

## Common Patterns for Specific Feature Types

### Documenting a new `_builtin/ffi/` wrapper

1. Add function to the table in `17-ffi.md` §17.8 "Available Wrappers"
2. Add function signature to `22-builtin-types.md` §22.13 "FFI Module Functions"
3. Add usage example in the relevant `17-ffi.md` subsection
4. If it introduces a new concept, add a pattern to `27-common-patterns.md`

### Documenting a new `:::` builtin

1. Add row to the builtin categories table in `17-ffi.md` §17.3
2. Document the semantics in the relevant feature section
3. If user-facing (not just compiler-internal), add to `22-builtin-types.md`

### Documenting a new class in `_builtin`

1. Add to `01-getting-started.md` `_builtin` directory listing (alphabetical order)
2. Add full type section to `22-builtin-types.md` with:
   - Brief description
   - Method table (Method | Signature | Description)
   - Code example
   - Cross-references to related chapters
3. If it's a primitive numeric type, add to `03-types.md` tables
