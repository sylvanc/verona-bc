use
{
  call_fn_ptr_ret_u64 = "call_fn_ptr_ret_u64"(ffi::ptr, u64): u64;
}

run_test(): u64
{
  var h = (x: u64): u64 -> x + 10;
  var cb = ffi::callback(h);
  var fn_ptr = cb();
  var result = :::call_fn_ptr_ret_u64(fn_ptr, 32);
  cb.free;
  result
}

main(): i32
{
  var r = 0;
  var result = callback_basic::run_test();
  if result != 42 { r = r + 1 }
  r
}
