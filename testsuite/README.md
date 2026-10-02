# Testsuite

The `compiler` suite registers named CTest DAGs from two explicitly owned
collection files:

| Collection | Pipeline |
| --- | --- |
| `vc-vbc.cmake` | Verona source -> VC -> VBC -> VBCI |
| `virc-vbc.cmake` | Textual VIR -> VIRC -> VBC -> VBCI |

Compiler feasibility and terminal stages are declared centrally in
`cmake/compiler_fixtures.cmake`.

Every node has `exit_code.txt`, `stdout.txt`, and `stderr.txt` goldens; silent
files are empty and exit-code files have no trailing newline.

Use `ninja update-dump` from `build/` to regenerate goldens and `ctest
--output-on-failure` to verify them.

Detailed guidance:

- [Architecture](docs/architecture.md)
- [Pipelines](docs/pipelines.md)
- [Fixtures](docs/fixtures.md)
- [Golden files](docs/golden-files.md)