// Regression test: passing a value through an FFI symbol whose Verona
// parameter type is a heterogeneous union.
//
// `i32 | none` is heterogeneous (i32 and none have different C representations)
// so `layout_type_id` reports `{ValueType::Dyn, ffi_type_value}`. At the
// FFI boundary a `Value` must cross as a pointer (`Value&`), not as a 16-byte
// struct-by-value, otherwise libffi over-reads the 8-byte scratch slot in
// `ffi_arg_vals`. On AArch64 the over-read is caught by ASAN; on x86-64 it is
// silent UB.

use
{
  printval = "printval"(i32 | none): none;
}

main(): none
{
  :::printval(42);
  :::printval(none);
}
