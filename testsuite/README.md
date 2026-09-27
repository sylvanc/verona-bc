# Testsuite

The testsuite registers named CTest DAGs from top-level collection files:

| Collection | Pipeline |
| --- | --- |
| `vc-vbc.cmake` | Verona source -> VC -> VBC -> VBCI |
| `vc-llvm.cmake` | Allowlisted Verona source -> VC -> LLVM -> native VRT |
| `virc-vbc.cmake` | Textual VIR -> VIRC -> VBC -> VBCI |
| `virc-llvm.cmake` | Allowlisted textual VIR -> VIRC -> LLVM -> native VRT |
| `vrt.cmake` | VRT API and internal tests |

LLVM collections register no tests when `VERONA_ENABLE_LLVM_BACKEND=OFF`.
Every node has `exit_code.txt`, `stdout.txt`, and `stderr.txt` goldens; silent
files are empty and exit-code files have no trailing newline. Native fixtures
state their coverage and non-goals in a source header or adjacent README.

Use `ninja update-dump` from `build/` to regenerate goldens and `ctest
--output-on-failure` to verify them.

Detailed guidance:

- [Pipelines](docs/pipelines.md)
- [Fixtures](docs/fixtures.md)
- [Golden files](docs/golden-files.md)
