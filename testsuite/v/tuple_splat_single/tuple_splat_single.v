// Test splat with 1 remaining element: scalar, not tuple.
// (let a, let rest...) = (1, 2) → rest = 2 (scalar i32)
main(): none
{
  let t = (i32 3, i32 5);
  (let a, let rest...) = t;
  // rest is i32, not a tuple.
  ffi::exit_code(a + rest)
}
