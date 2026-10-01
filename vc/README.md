# VC

VC compiles Verona source into reified VIR, then hands that tree directly to
the shared VIRC pipeline. It links `virc_core` and `virc_vbc` and emits VBC
without invoking the standalone VIRC executable or round-tripping through
textual VIR. VBC is the current VC output.

Frontend passes and language-specific structures live in `passes/`, `lang.h`,
and `lang.cc`. Built-in Verona definitions live in `_builtin/`.

See the [language documentation](docs/README.md),
[compiler pipeline](docs/20-compiler-pipeline.md),
[toolchain usage](docs/21-toolchain-usage.md).

## Development Notes

## To Do

Optimization.
- Allow inline of `raise` lambdas.
- Overall compile times.

Semantics.
- Region creation.
- Fix path compression, storing rank in Pending Location.
- For values in match cases, make it so that it's only a parameter if it has a type annotation, otherwise it's a value.
  - What about destructuring a pair?

Parser issue: `{}, ...` terminates the group the `{}` are in before the `,` starts a List, creating a weird structure.

FFI.
- Platform dependent code for dealing with libraries?

Reviews.
- A Verona writing skill? `vc` project scaffold that adds the skill and a project-specific CLAUDE.md?

Tuples.
- Flatten a tuple into another tuple.

Names.
- Allow looking down a type parameter.
- Can we look down an algebraic type (via an alias)?

Types.
- Better contradiction for type aliases.
- Intersections can fulfill shapes.
- Check recursion on type alias.
  - Does `alias[alias[A]]` cause both `A` and `alias[A]` to be bound to the same type parameter?
- Can type parameters take type arguments?

Packages:
- Asynchronous `stdin`.
- Hash map, hash set, ordered map, ordered set, list, deque, vector, span.
- CLI.
  - Needs a map, so do an RB tree or hash map.
  - Needs console I/O to print usage and errors.
  - Needs arguments, environment variables.
- File system.
- Network.
- Persistent collections.
- Sort.

Software engineering:
- Public, private.
- Proposal: `_` means private, public shapes can't have private methods.
- Can't `use` a private name, nor a public name inside a private name.
- Is `final` private?

Security:
- Integrate package management with signed and transparent statements and policies?
- LFI sandboxing?
- Control over FFI?

Encode `ValueParam` as types.
- Need an "equivalence" relation at compile time for invariant sub-typing.
- This may require execution during compilation.

Optimization:
- Treat parameter types that are shapes as implicit type parameters.
- Compile time evaluation.
