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

* Merge, freeze, extract.
  * Use `location` to store SCC information.
  * Modes that allow/disallow parent pointers and stack RC.
* Types.
  * Raise and throw signatures on functions.
  * Function types? Not strictly needed, as this can be encoded as objects.
  * Cache type check results? Would also prevent circular type checks.
  * `imm` and other memory location types?
* Initializing global values.
  * How to clean them up?
* Embedded fields (objects and arrays).
  * Embed with no header, use `snmalloc::external_pointer`.
  * Need a different `ValueType` to avoid doing this for all objects/arrays.
  * Compatible with FFI.
  * Can't store? Or is a store a copy? How do we initialize the field?
* Make a `bool[]` have 1-bit instead of 8-bit elements.
* Math ops for numeric limits, by type?
* FFI with `libffi`.
  * Can we wrap returned `struct` as objects?
  * Platform-specific FFI. Only load for the runtime platform.
* I/O with `libuv`.
  * Do everything asynchronously.
  * Yield the current thread when waiting for I/O.
  * Need more than one Thread per scheduler thread.
* Sockets.
  * TCP/TLS server?
  * UDP?
  * Cloudflare `quiche` for `QUIC`?
* Introspection.
  * Get a value's dynamic type.
  * Functions: get the argument count and types, and the return type.
  * Classes:
    * Get the field count, types, and names.
    * Get the method count and names.
* Mark classes to auto-generate a C API.
  * Output a C header file.
  * Allow calling back into the interpreter.
* Generate FFI stubs automatically from C/C++ headers.
* Interactive debugger.
* Build an DAP/LSP to allow debugging.
* AST to IR output.
* Compile to LLVM IR and/or Cranelift.
* Hot patching running code.
* Serialize a behavior to execute on another process or machine.
