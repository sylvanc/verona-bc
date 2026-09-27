# Test Fixtures

Source fixtures live under `testsuite/v/`, textual VIR fixtures under
`testsuite/vir/`, and direct runtime fixtures under `testsuite/vrt/`.

Compiler fixtures must be self-contained and may use only implicitly available
`_builtin` definitions. Every canonical fixture receives effective
`VBC_STAGE` and `LLVM_STAGE` values from defaults, anchored rules, and optional
explicit source groups and exact overrides in
`cmake/compiler_fixtures.cmake`. Compile-error cases normally inherit
`VBC_STAGE compile` from the `compile_only/` rule.

Every non-trivial LLVM or VRT fixture states the behavior it covers and its
non-goals in a short source header. Long multi-scenario fixtures may place that
detail in an adjacent README and link it from the source. Coverage text must
describe assertions the fixture actually performs, not intended future work.

Fixture names describe behavior rather than backend eligibility. Compiler
fixtures that intentionally exercise VRT remain under `v/` or `vir/`; direct
C/C++ VRT fixtures remain under `vrt/`. See [Testsuite
Architecture](architecture.md) for metadata and labeling rules.