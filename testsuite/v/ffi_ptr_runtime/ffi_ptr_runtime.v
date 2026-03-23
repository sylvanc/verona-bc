use
{
  write_all = "write_all"(i32, ffi::ptr, usize): bool;
  call_fn_ptr_ret_u64 = "call_fn_ptr_ret_u64"(ffi::ptr, u64): u64;
}

main(): i32
{
  var result = 0;

  let data = array[u8]::fill 1;
  if !:::write_all(1, data, 0)
  {
    result = result + 1
  }

  let cb = ffi::callback (x: u64): u64 ->
  {
    x + 1
  }

  let value = :::call_fn_ptr_ret_u64(cb.raw, 41);

  if value != 42
  {
    result = result + 2
  }

  result
}
