# Test Fixtures

Verona source fixtures live under `testsuite/v/`, and textual VIR fixtures live
under `testsuite/vir/`.

Compiler fixtures must be self-contained and may use only implicitly available
`_builtin` definitions. Every canonical fixture receives an effective
`VBC_STAGE` from defaults, anchored rules, and optional exact overrides in
`cmake/compiler_fixtures.cmake`. Compile-error cases normally inherit
`VBC_STAGE compile` from the `compile_only/` rule.

Fixture source names match their parent directory names. Each fixture stores
its compile and run expectations below a same-named output directory so the
registered graph and committed goldens have one stable root.

See [Testsuite Architecture](architecture.md) for metadata and labeling rules.