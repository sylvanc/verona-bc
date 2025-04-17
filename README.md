# Verona BC

This is an experimental byte code interpreter for the Verona operational semantics.

## Non-Local Returns

A function call can be made as a `call`, a `subcall`, or a `try`. Functions can return via a `return`, a `raise`, or a `throw`

If you make a `call` and the function does a `return`, your destination register will have the returned value. If the function does a `raise`, you will immediately `return` the raised value. If the function does a `throw`, you will immediately `throw` the thrown value.

If you make a `subcall` and the function does a `return`, your destination register will have the returned value. If the function does a `raise`, you will immediately `raise` the raised value. If the function does a `throw`, you will immediately `throw` the thrown value.

If you make a `try`, your destination register will have the returned value regardless of whether the function does a `return`, a `raise`, or a `throw`.

One way to use this is to implement Smalltalk style non-local returns. To do so, functions `call` blocks and other functions, and `return` results, whereas blocks `subcall` functions and other blocks, and either `raise` to return from the calling function (popping all blocks), or `return` to return to the calling block or function.

Another way to use this is to implement exceptions. To do so, `throw` exception values. Exceptions are caught by a `try`.

## To-Do List

* A register in a label may get initialized in a label that comes after it.
  * Similarly, a register in a label may appear to have been initialized in a preceding label, but that preceding label may not be executed.
* PCs could be 32 bits, which gives up to 16 GB of code.
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
* Command line arguments.
* Standard IO? Does this need string support?
* File API? Seems like too much.
* Some kind of `dlopen` system for adding FFI?
* Debug info that maps instructions to file:line:column?
* Compile byte code to LLVM IR and/or Cranelift.
* AST to IR output.
