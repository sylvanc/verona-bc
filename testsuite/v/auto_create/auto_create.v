// Test: auto-generated create function for classes without an explicit one.
// The compiler should generate a `create` that takes parameters matching
// the class fields and calls `new` with them.

point
{
  x: i32;
  y: i32;
}

main(): none
{
  // Uses the auto-generated create(x: i32, y: i32): point
  let p = point(3.i32, 4.i32);
  ffi::exit_code(p.x + p.y)
}
