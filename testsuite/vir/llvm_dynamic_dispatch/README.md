# LLVM dynamic dispatch fixture

This fixture verifies lookup-based ordinary and tail dispatch through both the
bytecode and native LLVM pipelines.

## Coverage

- Method lookup on a receiver whose type is a union of two nominal classes.
- Ordinary dynamic calls and dynamic tail calls through the lookup result.
- Distinct method implementations that share one native signature.
- Optimizer alpha-renaming of lookup results after helper inlining.
- Exclusion of an unrelated same-named method with an incompatible signature.

## Native VRT coverage

VRT allocates the receiver objects and consumes their emitted class and method
metadata during dynamic lookup. The selected generated method is then invoked
through both ordinary-call and tail-call paths.

## Non-goals

The fixture does not test missing methods, signature mismatches at a selected
call site, inheritance, object fields, or dispatch failure reporting.