# Instruction Dispatch

`Thread::step()` reads one `vbc::Op` from the active frame and decodes its
operands from `Program`. Opcode handlers update registers, regions, frames, or
the scheduler and leave the frame program counter at the next instruction.

Dispatch uses neutral opcode identities from `include/vbc/format.h`; operand
execution remains private to VBCI. Debug builds can trace decoded instructions
through `Thread::trace_instruction()`.

Adding an opcode requires coordinated producer, loader, dispatch, and format
updates.