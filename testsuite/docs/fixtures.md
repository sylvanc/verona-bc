# Test Fixtures

Verona source fixtures live under `testsuite/v/`, and textual VIR fixtures live
under `testsuite/vir/`.

Compiler fixtures must be self-contained and may use only implicitly available
`_builtin` definitions. Compile-error cases belong under `v/compile_only/` and
have no run graph.

Fixture source names match their parent directory names. Each fixture stores
its compile and run expectations below a same-named output directory so the
registered graph and committed goldens have one stable root.