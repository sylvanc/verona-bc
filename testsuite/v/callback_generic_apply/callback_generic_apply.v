genapply[T]
{
  apply(self: genapply[T], x: T): T
  {
    x
  }
}

main(): i32
{
  var obj = genapply[i32];
  var cb = :::make_callback(obj);
  :::free_callback(cb);
  0
}
