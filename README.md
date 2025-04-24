# Verona BC

This is an experimental byte code interpreter for the Verona operational semantics.

## Non-Local Returns

A function call can be made as a `call`, a `subcall`, or a `try`. Functions can return via a `return`, a `raise`, or a `throw`

If you `call` and the function does a `return`, your destination register will have the returned value. If the function does a `raise`, you will immediately `return` the raised value. If the function does a `throw`, you will immediately `throw` the thrown value.

If you `subcall` and the function does a `return`, your destination register will have the returned value. If the function does a `raise`, you will immediately `raise` the raised value. If the function does a `throw`, you will immediately `throw` the thrown value.

If you `try`, your destination register will have the returned value regardless of whether the function does a `return`, a `raise`, or a `throw`.

One way to use this is to implement Smalltalk style non-local returns. To do so, functions `call` blocks and other functions, and `return` results, whereas blocks `subcall` functions and other blocks, and either `raise` to return from the calling function (popping all blocks), or `return` to return to the calling block or function.

Another way to use this is to implement exceptions. To do so, `throw` exception values. Exceptions are caught by a `try`.

## Debug Info

Debug info is appended to the end of the byte code.

The first entry is a string table. This is:
* A ULEB128 count of the number of strings.
* Strings, each of which is a ULEB128 length followed by the string.

This is followed by a ULEB128 string table index for the compilation path.

This is followed by debug info for each user-defined classes. This is:
* A ULEB128 string table index for the class name.
* A ULEB128 string table index for each field name.
* A ULEB128 string table index for each method name.

This is followed by debug info for each function. This is:
* A ULEB128 string table index for the function name.
* A ULEB128 string table index for each register name.
* A debug info program.

A debug info program is a sequence of instructions encoded as ULEB128s. The low 2 bits are the instruction, and the high bits are the argument. The instructions are:
* `vbci::File` (0): the argument is the source file's name in the string table, reset the offset to 0.
* `vbci::Offset` (1): advance the offset by the argument, advance the PC by 1.
* `vbci::Skip` (2): advance the PC by the argument.

## To-Do List

* Type test.
  * Add ClassIds as types.
  * Add union, array, ref, cown, imm, etc. types?
  * How do subtype checks work? Entirely as union types?
  * Automatic checking on calls, returns, and stores?
* Mode to skip checking argument counts?
  * Separate for static and dynamic calls?
  * Could do this in vbcc instead of in vbci.
* Audit all the errors in `ident.h`.
* Embedded fields.
* Initializing globals.
* Merge, freeze, extract.
  * Use `location` to store SCC information.
  * Modes that allow/disallow parent pointers and stack RCs.
* When.
* Compact objects and arrays when a field type or content type can be represented as a single value, e.g., an array of `u8`.
* Command line arguments.
* Change to 8-bit field ID in `Ref`?
  * Or provide "long `arg`" for initializing huge objects?
  * General purpose "long register" versions of all instructions?
  * Would allow functions to have semi-unlimited register counts.
* Math ops for numerical limits, by type?
* Standard IO? Does this need string support?
* String constants? `u8[]`?
* File API? Seems like too much.
* Some kind of `dlopen` system for adding FFI?
  * Provide the DSO to vbcc as well as vbci.
  * VIR specifies the DSOs to load.
  * The DSO has an entry point that returns a list of function names, with their parameter counts.
  * These functions take vbci::Value arguments and return a vbci::Value.
* Build an DAP/LSP to allow debugging.
* AST to IR output.
* Compile to LLVM IR and/or Cranelift.
