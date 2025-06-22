# Verona BC

This is an experimental byte code interpreter for the Verona operational semantics.

## Types

Union types can only be on the right-hand side of a `type` definition. If you need a union type somewhere else, create a `type` for it.

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

* Local region.
  * Can't send a local region object - auto-move to a fresh region?
    * Don't just call `is_sendable` in thread `when` handler.
    * Do delayed-send?
      * Allow creating a behavior with `exec_count_down` 1 higher.
      * Expose a decrement function for that.
      * Set the region parent to be the behavior.
      * When stack RC goes to 0, decrement the behavior's `exec_count_down`.
* Do programs ever need to create null pointers?
* Is it ok to immortalize a stack allocated object? Seems like no?
* Merge, freeze, extract.
  * Delayed freeze?
  * Modes that allow/disallow parent pointers and stack RC?
  * When freezing a region with a stack RC, how do we know which SCC have those additional incoming edges?
    * In an RC region, can we reuse the existing object RC?
    * No way to do it in a non-RC region.
* Types.
  * Raise and throw signatures on functions.
  * Function types? Not strictly needed, as this can be encoded as objects.
  * Cache type check results? Would also prevent circular type checks.
  * `imm` and other memory location types?
* Initializing global values.
  * How to clean them up?
  * Immortal global values (compile-time generated) could pack their object identity into the Location field, to avoid any use of the memory address.
* Embedded fields (objects and arrays).
  * Embed with no header, use `snmalloc::external_pointer`.
  * Need a different `ValueType` to avoid doing this for all objects/arrays.
  * Compatible with FFI.
  * Can't store? Or is a store a copy? How do we initialize the field?
* Make a `[bool]` have 1-bit instead of 8-bit elements.
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
