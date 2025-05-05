# Verona BC

This is an experimental byte code interpreter for the Verona operational semantics.

## Dynamic Types

To allow a fixed size type representation:
* You can't have an array of `array T`. To do this, create a `typedef` for `array T`, and have an array of that `typedef`.
* You can't have a ref of `ref T`. That means fields, `cown` contents, and array elements can't be of type `ref T`. To do this, create a `typedef` for `ref T`, and use that.
* You can't have a `cown` of `cown T`. That means `cown` contents can't be of type `cown T`. To do this, create a `typedef` for `cown T` and use that.

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
  * `struct` types.
    * How does a `struct` return work? Can't use `ffi_arg`?
    * Can we pass objects as `struct` to FFI?
      * Won't work for embedded `struct`, would have to pack the fields.
      * Won't work for pointers unless we store pointers offset from the header.
    * Can we wrap returned `struct` as objects?
  * Allow a version string on a symbol, use `dlvsym`.
  * Platform-specific FFI. Only load for the runtime platform.
  * FFI to get the runtime platform.
* Types.
  * Raise and throw signatures on functions.
  * Function types? Not strictly needed, as this can be encoded as objects.
  * Cache type check results? Would also prevent circular type checks.
  * `imm` and other memory location types?
* Initializing global values.
* Introspection.
  * Get a value's dynamic type.
  * Functions: get the argument count and types, and the return type.
  * Classes:
    * Get the field count, types, and names.
    * Get the method count and names.
* Interactive debugger.
* Embedded fields.
  * Embedded arrays with a constant size.
* Merge, freeze, extract.
  * Use `location` to store SCC information.
  * Modes that allow/disallow parent pointers and stack RCs.
* When.
* General purpose "long register" versions of all instructions?
  * Consider ULEB128 for the code instead of fixed length.
  * Would allow functions to have semi-unlimited register counts.
* Math ops for numerical limits, by type?
* String constants? `u8[]`?
* Mark classes to auto-generate a C API.
  * Output a C header file.
  * Allow calling back into the interpreter.
* Build an DAP/LSP to allow debugging.
* AST to IR output.
* Compile to LLVM IR and/or Cranelift.
* Serialize a behavior to execute on another process or machine.
