multiapply
{
  apply(self: multiapply, x: i32): i32
  {
    x
  }

  apply(self: multiapply, x: u64): u64
  {
    x
  }
}

main(): i32
{
  var obj = multiapply;
  var cb = ffi::callback(obj);
  0
}
