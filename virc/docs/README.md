# VIRC Internals

VIRC validates and analyzes reified VIR and builds output-neutral compilation
state for peer emitters.

- [Repository-internal API](api.md)
- [Shared pipeline](pipeline.md)
- [Compilation model](compilation-model.md)
- [Type system](type-system.md)
- [Diagnostics](diagnostics.md)

The [VIR format](../../docs/formats/vir.md) owns the input contract. Backend
implementation details are documented under
[VBC](../vbc/README.md) and [LLVM](../llvm/README.md).