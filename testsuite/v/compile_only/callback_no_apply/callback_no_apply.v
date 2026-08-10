noapply
{
  x: i32;
}

main(): none
{
  var obj = noapply(0);
  var cb = ffi::callback(obj);
}
