main(): none
{
  let f = (x: i32) -> x + 1;
  ffi::exit_code(f(41))
}
