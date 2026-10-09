# Test Pipelines

The `compiler` suite registers VBC dependency graphs by frontend:

| Collection | Pipeline |
| --- | --- |
| `vc-vbc.cmake` | Verona source -> VC -> VBC -> VBCI |
| `virc-vbc.cmake` | Textual VIR -> VIRC -> VBC -> VBCI |

CTest node names remain stable independently of collection filenames. Build
and run from the canonical `build/` directory with `ninja install` followed by
`ctest --output-on-failure`.

`cmake/compiler_fixtures.cmake` declares each source's terminal `VBC_STAGE`.
The collection registers all prerequisite stages through that point.