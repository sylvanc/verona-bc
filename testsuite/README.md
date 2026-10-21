# Testsuite

The testsuite registers named CTest DAGs in two explicitly owned suites:

| Suite | Collections | Ownership |
| --- | --- | --- |
| `compiler` | `vc-vbc.cmake`, `vc-llvm.cmake`, `virc-vbc.cmake`, `virc-llvm.cmake` | End-to-end compiler pipelines |
| `runtime` | `vrt.cmake` | Direct VRT ABI, API, behavior, and internal tests |

Compiler feasibility and terminal stages are declared centrally in
`cmake/compiler_fixtures.cmake`. LLVM collections register no tests when
`VERONA_ENABLE_LLVM_BACKEND=OFF`.
Every node has `exit_code.txt`, `stdout.txt`, and `stderr.txt` goldens; silent
files are empty and exit-code files have no trailing newline. Native fixtures
state their coverage and non-goals in a source header or adjacent README.

Use `ninja update-dump` from `build/` to regenerate goldens and `ctest
--output-on-failure` to verify them.

Detailed guidance:

- [Architecture](docs/architecture.md)
- [Pipelines](docs/pipelines.md)
- [Fixtures](docs/fixtures.md)
- [Golden files](docs/golden-files.md)
