box
{
  p: ffi::ptr;
}

main(): none
{
  var x: i32 = 42;
  let p = ffi::ptr::create(x);
  let b = box::create(p);
}
