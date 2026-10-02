# Compilation Model

**Current:** `virc::Compilation` in `virc/model/compilation.h` is the shared,
output-neutral result of VIRC analysis. It retains the VIR root and source
paths, indexed definitions, function analysis state, type records, and
typechecker environments.

`FuncState` maps symbolic labels and locals to dense indices and stores
liveness state. `TypeInfo` represents compound arrays, cowns, references,
unions, and tuples using other type IDs. Name tables intern execution and debug
strings separately.

The model does not contain LLVM objects, VBCI values, or VRT implementation
types. VBC encoding of this state is described under the
[VBC emitter](../vbc/README.md); LLVM representation choices are local to the
[LLVM emitter](../llvm/README.md).