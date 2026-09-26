# VBC Lowering

**Current:** `instruction_encoder.cc` translates validated VIR statements and
terminators to VBC opcodes. It resolves symbolic function, method, field,
label, and local IDs through `Compilation` and `FuncState` before writing
operands.

Move and copy arguments are emitted before their call or allocation opcode.
Constants use their validated primitive type, and region allocations map VIR
region nodes to `vbc::RegionType`. Memo loads resolve the initialization slot
recorded by the shared VIRC pipeline.

Opcode meaning and operand order are part of the
[VBC format](../../../docs/formats/vbc.md). This document describes emitter
ownership, not a second wire specification.