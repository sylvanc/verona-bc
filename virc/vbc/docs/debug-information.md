# VBC Debug Information

**Current:** `DebugInfo` tracks source files, source offsets, and instruction
program counters while functions are encoded. Synthetic statements with no
source origin do not create automatic locations. Explicit VIR `Source` and
`Offset` statements override automatic locations for their function.

Debug operations encode file changes, offset changes, and skipped instruction
bytes. The debug string table, source contents, and operations are assembled
after the instruction stream and compressed with Zstandard. Stripped output
omits the complete debug section.

The operation bit layout is specified by `vbc::DIOp` and documented in the
[VBC format](../../../docs/formats/vbc.md).