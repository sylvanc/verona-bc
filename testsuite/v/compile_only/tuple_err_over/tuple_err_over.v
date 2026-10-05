// Test error: too many bindings for tuple (3 bindings, 2 elements).
// Should produce a compile error.
main(): none
{
  let t = (i32 3, i32 5);
  (let a, let b, let c) = t;
  ffi::exit_code(a + b + c)
}
