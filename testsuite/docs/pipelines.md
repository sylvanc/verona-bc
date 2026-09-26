# Test Pipelines

The shared VBC collection files register CTest dependency graphs by input and
output pipeline:

| Collection | Pipeline |
| --- | --- |
| `vc-vbc.cmake` | Verona source -> VC -> VBC -> VBCI |
| `virc-vbc.cmake` | Textual VIR -> VIRC -> VBC -> VBCI |

CTest node names remain stable independently of collection filenames. Build
and run from the canonical `build/` directory with `ninja install` followed by
`ctest --output-on-failure`.