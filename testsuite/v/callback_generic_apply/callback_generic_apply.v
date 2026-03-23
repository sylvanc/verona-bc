use
{
  call_fn_ptr_ret_u64 = "call_fn_ptr_ret_u64"(ffi::ptr, u64): u64;
}

genapply[T]
{
  apply(self: genapply[T], x: T): T
  {
    x
  }
}

run_test(): u64
{
  var obj = genapply[u64];
  var cb = ffi::callback(obj);
  var fn_ptr = cb.raw;
  var result = :::call_fn_ptr_ret_u64(fn_ptr, 42);
  result
}

main(): i32
{
  var r = 0;
  var result = callback_generic_apply::run_test();
  if result != 42 { r = r + 1 }
  r
}
