# Test Pipelines

The `compiler` suite registers CTest dependency graphs by frontend and backend:

| Collection | Pipeline |
| --- | --- |
| `vc-vbc.cmake` | Verona source -> VC -> VBC -> VBCI |
| `vc-llvm.cmake` | Eligible Verona source -> VC -> LLVM -> native VRT |
| `virc-vbc.cmake` | Textual VIR -> VIRC -> VBC -> VBCI |
| `virc-llvm.cmake` | Eligible VIR -> VIRC -> LLVM -> native VRT |

The separate `runtime` suite uses `vrt.cmake` for direct VRT fixtures.

`cmake/compiler_fixtures.cmake` defines each source's terminal `VBC_STAGE` and
`LLVM_STAGE`. Collections register all prerequisite stages through that point.
LLVM collections return without registering tests when
`VERONA_ENABLE_LLVM_BACKEND=OFF`.

CTest node names remain stable independently of collection filenames. Build
and run from the canonical `build/` directory with `ninja install` followed by
`ctest --output-on-failure`.

`reference`, for example, is eligible for both backends and checks one program
through VBCI and native VRT. `finalizer` compiles through VBC but runs only
through LLVM because its runtime-error behavior is VRT-specific. Fixture names
describe behavior; backend eligibility belongs only in the manifest.