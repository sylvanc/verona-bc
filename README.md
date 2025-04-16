# Verona BC

This is an experimental byte code interpreter for the Verona operational semantics.

## To-Do List

* Check that global IDs in operations are valid.
* A register in a label may get initialized in a label that comes after it.
  * Similarly, a register in a label may appear to have been initialized in a preceding label, but that preceding label may not be executed.
* Compile VIR to byte code.
* VIR for "lambda" call and try.
* Type checking in the byte code?
* Object, cown, and region finalization and freeing.
* Stack allocation that's unwound when a frame is popped.
* Initializing globals.
* An opcode for creating a ref that consumes the source?
* Math ops for numerical limits, by type?
* Type test.
* Merge, freeze, extract.
* When.
* Arrays.
* Standard IO? Does this need string support?
* File API? Seems like too much.
* Some kind of `dlopen` system for adding FFI?
* Debug info that maps instructions to file:line:column?
* Compile byte code to LLVM IR and/or Cranelift.
