# Verona BC Toolchain

This repository contains an experimental Verona compiler and runtime
toolchain. VC lowers Verona source to Verona Intermediate Representation
(VIR), and VIRC validates and analyzes VIR before a peer emitter produces
Verona Bytecode (VBC) or LLVM IR.

**Current:** VBC is the default output and runs on VBCI. LLVM IR emission is
available when `VERONA_ENABLE_LLVM_BACKEND=ON` and targets VRT.

**Migration:** VRT does not yet replace VBCI for every runtime subsystem. The
[runtime migration policy](docs/architecture/vbci-vrt-migration.md) records
the boundary and sequencing rules.

## Quick Start

Configure and build in the `build` directory, then run the test suite:

```sh
cmake -S . -B build -G Ninja
ninja -C build install
ctest --test-dir build --output-on-failure
```

Build and run the hello fixture through the default VBC path:

```sh
cd build
dist/vc/vc build ../testsuite/v/hello
dist/vbci/vbci hello.vbc
```

Set `-DVERONA_ENABLE_LLVM_BACKEND=OFF` while configuring when LLVM is not
available. See [Toolchain Usage](vc/docs/21-toolchain-usage.md) for output
selection and standalone VIRC commands.

## Components

| Component | Responsibility |
| --- | --- |
| [VC](vc/README.md) | Verona frontend and source-to-VIR lowering |
| [VIRC](virc/README.md) | Shared VIR validation, analysis, and output-neutral compilation state |
| [VBCI](vbci/README.md) | VBC loader and interpreter runtime |
| [VRT](vrt/README.md) | Native runtime and public C ABI used by LLVM output |
| [Interchange formats](docs/formats/README.md) | VIR and VBC contracts and compatibility |
| [Testsuite](testsuite/README.md) | Source, VIR, VBC, LLVM, and runtime test pipelines |

The current architecture and accepted decisions are indexed in
[Architecture](docs/architecture/README.md). Verona language documentation is
under [VC Language Documentation](vc/docs/README.md), and documentation
ownership is defined by the [Documentation Policy](docs/documentation-policy.md).
The complete contributor and user documentation map is in
[Documentation](docs/README.md).
