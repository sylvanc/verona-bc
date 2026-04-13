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

tester
{
  cb: ffi::callback[u64->u64];
  result: u64;

  create(): tester
  {
    let h = genapply[u64];
    let cb = ffi::callback h;
    new { cb, result = 0 }
  }

  run(self: tester, x: u64): none
  {
    let fn_ptr = self.cb.raw;
    self.result = :::call_fn_ptr_ret_u64(fn_ptr, x);
  }
}

main(): none
{
  var r = 0;
  let t = tester;
  t.run 42;
  if t.result != 42 { r = r + 1 }
  ffi::exit_code r
}
