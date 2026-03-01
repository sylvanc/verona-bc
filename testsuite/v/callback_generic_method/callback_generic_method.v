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
  var cb = :::make_callback(obj);
  :::free_callback(cb);
  0
}
