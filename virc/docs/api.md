# VIRC API

**Current:** the API is repository-internal and declared in `virc/compile.h`.
`virc::compile(Node)` owns a new `Compilation`, runs the shared pipeline, and
returns `CompileResult` with the Trieste `ProcessResult` and shared state.

```cpp
auto result = virc::compile(reified_vir);
if (result)
  virc::vbc::emit(result.compilation(), output);
```

Emitters may derive output but must not mutate shared compilation state.

**Planned:** no public embedding compatibility is promised. There is
intentionally no `include/virc.h`; promotion to an installed API requires a
separate compatibility decision.