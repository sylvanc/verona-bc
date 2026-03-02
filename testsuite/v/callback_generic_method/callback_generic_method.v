genmethod
{
  apply[T](self: genmethod, x: T): T
  {
    x
  }
}

main(): i32
{
  var obj = genmethod;
  var cb = ffi::callback(obj);
  cb.free;
  0
}
