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

* FFI with `libffi`.
  * Allow a version string on a symbol, use `dlvsym`.
  * `struct` types.
* Type test.
  * Raise and throw signatures on functions.
  * Add `ref`, `cown` types.
  * Cache type check results? Would also prevent circular type checks.
  * `imm` and other memory location types?
* Compact objects and arrays when a field type or content type can be represented as a single value, e.g., an array of `u8`.
* FFI to access command line arguments.
* Initializing globals.
* Introspection.
  * Get the dynamic type of a value.
  * Functions: get the argument count and types, and the return type.
  * Classes:
    * Get the field count, types, and names.
    * Get the method count and names.
* Interactive debugger.
* Embedded fields.
* Merge, freeze, extract.
  * Use `location` to store SCC information.
  * Modes that allow/disallow parent pointers and stack RCs.
* When.
* General purpose "long register" versions of all instructions?
  * Would allow functions to have semi-unlimited register counts.
  * Argument space is highest of:
    * Parameter count of any function.
    * Parameter count of any FFI symbol.
    * Field count of any class.
* Math ops for numerical limits, by type?
* String constants? `u8[]`?
* Build an DAP/LSP to allow debugging.
* AST to IR output.
* Compile to LLVM IR and/or Cranelift.
* Serialize a behavior to execute on another process or machine.
