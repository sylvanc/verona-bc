genmethod
{
  apply[T](self: genmethod, x: T): T
  {
    x
  }
}

main(): none
{
  var obj = genmethod;
  var cb = ffi::callback(obj);
}
