box
{
  p: ffi::ptr;
}

main(): i32
{
  var x: i32 = 42;
  let p = ffi::ptr::create(x);
  let b = box::create(p);
  0
}
