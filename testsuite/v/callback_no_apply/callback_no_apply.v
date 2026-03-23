noapply
{
  x: i32;
}

main(): i32
{
  var obj = noapply(0);
  var cb = ffi::callback(obj);
  0
}
