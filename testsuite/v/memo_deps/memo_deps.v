// Test: once function that calls another once function.
// Verifies dependency ordering works correctly.

once base(): i32
{
  i32 10
}

once derived(): i32
{
  memo_deps::base() + i32 32
}

main(): none
{
  let v = memo_deps::derived();
  ffi::exit_code(if v == i32 42 { i32 0 } else { i32 1 })
}
