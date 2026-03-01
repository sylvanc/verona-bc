noapply
{
  x: i32;
}

main(): i32
{
  var obj = noapply(0);
  var cb = :::make_callback(obj);
  :::free_callback(cb);
  0
}
