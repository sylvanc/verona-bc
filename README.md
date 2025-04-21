# Verona BC

This is an experimental byte code interpreter for the Verona operational semantics.

## Non-Local Returns

A function call can be made as a `call`, a `subcall`, or a `try`. Functions can return via a `return`, a `raise`, or a `throw`

If you `call` and the function does a `return`, your destination register will have the returned value. If the function does a `raise`, you will immediately `return` the raised value. If the function does a `throw`, you will immediately `throw` the thrown value.

If you `subcall` and the function does a `return`, your destination register will have the returned value. If the function does a `raise`, you will immediately `raise` the raised value. If the function does a `throw`, you will immediately `throw` the thrown value.

If you `try`, your destination register will have the returned value regardless of whether the function does a `return`, a `raise`, or a `throw`.

One way to use this is to implement Smalltalk style non-local returns. To do so, functions `call` blocks and other functions, and `return` results, whereas blocks `subcall` functions and other blocks, and either `raise` to return from the calling function (popping all blocks), or `return` to return to the calling block or function.

Another way to use this is to implement exceptions. To do so, `throw` exception values. Exceptions are caught by a `try`.

## To-Do List

* Regions.
  * Alloc/free depends on region type.
  * Can't free a region until all objects in it have been freed.
* Type test.
  * Add ClassIds as types.
  * Add union, array, ref, cown, imm, etc. types?
  * How do subtype checks work? Entirely as union types?
  * Automatic checking on calls and stores?
* Mode to skip checking argument counts?
  * Separate for static and dynamic calls?
  * Could do this in vbcc instead of in vbci.
* Audit all the errors in `ident.h`.
* Embedded fields.
* Initializing globals.
* Merge, freeze, extract.
  * Use `location` to store SCC information.
  * Modes that allow/disallow parent pointers and stack RCs.
* Object, cown, and region finalization and freeing.
* When.
* Compact objects and arrays when a field type or content type can be represented as a single value, e.g., an array of `u8`.
* Command line arguments.
* Change to 8-bit field ID in `Ref`?
  * Or provide "long `arg`" for initializing huge objects?
* Math ops for numerical limits, by type?
* Standard IO? Does this need string support?
* String constants? `u8[]`?
* File API? Seems like too much.
* Some kind of `dlopen` system for adding FFI?
  * Provide the DSO to vbcc as well as vbci.
  * VIR specifies the DSOs to load.
  * The DSO has an entry point that returns a list of function names, with their parameter counts.
  * These functions take vbci::Value arguments and return a vbci::Value.
* Debug info that maps instructions to file:line:column?
  * Debug adapter protocol?
* Build an LSP to allow debugging.
* Compile to LLVM IR and/or Cranelift.
* AST to IR output.
