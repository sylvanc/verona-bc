# LLVM dynamic dispatch fixture

This fixture verifies lookup-based ordinary and tail dispatch through both the
bytecode and native LLVM pipelines.

## Coverage

- Method lookup on a receiver whose type is a union of two nominal classes.
- Class-ID switch lowering with direct generated function descriptors and a
	default `vrt_object_lookup` path.
- Ordinary dynamic calls and dynamic tail calls through the lookup result.
- Distinct method implementations that share one native signature.
- Optimizer alpha-renaming of lookup results after helper inlining.
- Exclusion of an unrelated same-named method with an incompatible signature.

## Native VRT coverage

VRT initializes the immortal singleton receivers and returns their immutable
class IDs to the generated switch. The selected generated function descriptor
is then consumed through both ordinary-call and tail-call paths. The runtime
lookup fallback is present in emitted IR but is not taken by this fixture's
known receiver types.

## Non-goals

The fixture does not test the large-union runtime lookup path, missing methods,
signature mismatches at a selected call site, inheritance, object fields, or
dispatch failure reporting.