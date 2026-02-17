# Verona Compiler (vc) Specifics

- **Build / test workflow**: `ninja -j$(nproc) vc` builds the compiler. `ninja install` puts the binary at `build/dist/vc/vc` with `_builtin` directory alongside it. `ninja test` runs the full test suite. The build binary (`build/vc/vc`) does NOT have `_builtin` next to it.
- **Debugging**: Use `lldb-20` for debugging segfaults and crashes.
- **Pass-limited testing**: `-p <passname>` stops after a specific pass (e.g., `-p ident`). `--dump_passes=<dir>` dumps intermediate ASTs.
- **`_builtin` resolution**: The parser's postparse hook looks for `_builtin` at `executable.parent_path() / "_builtin"`. Only the installed binary has it.
- **`use` semantics**: `use X` imports X's contents for unqualified lookup within the current scope, but NOT for qualified lookdown from outside. A `Use` node with `[Include]` creates an include entry in the parent's symtab, which is followed by `lookup()` but not `lookdown()`.
- **Fully qualified names**: After the ident pass, all TypeName/FuncName nodes contain only NameElement children — no relative references. Every name is fully qualified from Top, using scope names with empty TypeArgs for intermediate scopes (even if they have TypeParams). This means names can be resolved without knowing the enclosing scope.
- **Ident pass architecture**: Uses `NodeWorker<Resolver>` with a `Processor` class. Key methods: `resolve_first()` (walks up scopes via lookup, builds FQ prefix via `build_fq_prefix()`), `resolve_down()` (looks into ClassDef via lookdown), `resolve_last()` (final type checks), `resolve_alias()` (follows TypeAlias chains), `concat()` (substitutes TypeParam references using a scope-to-prefix-elem map built from the prefix path), `find_def()` (navigates a fully qualified path from Top).
- **Self-referential includes**: When resolving a TypeName inside a `Use` node, `lookup()` can return the `Use` node itself via the include mechanism. The ident pass skips these with `n->parent() == def`.
- **Reify pass**: `get_reification()` navigates FQ names from `top`. Intermediate FQ prefix elements may have empty TypeArgs even when the scope has TypeParams — only build substitution when args are provided.
