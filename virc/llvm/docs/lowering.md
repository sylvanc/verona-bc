# LLVM Lowering

**Current:** `LLVMCodegen` consumes a `const Compilation&` and builds one LLVM
module. Its stages configure the target, declare and define class layouts,
declare callables and VRT functions, emit metadata and functions, build program
initialization, then verify and write textual LLVM IR.

Statement and terminator dispatch is split by VIR operation under
`statements/`, `calls/`, and `terminators/`. Unsupported forms fail emission
with a source-associated diagnostic; they do not fall back to VBC behavior.

Reference statements are grouped under `statements/references/` while
preserving the VIR token boundary: `register_ref.cc`, `field_ref.cc`,
`array_ref.cc`, `array_ref_const.cc`, `load.cc`, and `store.cc` each own one
top-level emitter. Field, array, and store lowerers share argument-transfer
handling through `LocalState` and then call one consuming VRT operation. They
do not reproduce region or write-barrier policy in LLVM IR.

The backend is optional and exists only when `VERONA_ENABLE_LLVM_BACKEND=ON`.
Shared semantics and IDs remain owned by VIRC.