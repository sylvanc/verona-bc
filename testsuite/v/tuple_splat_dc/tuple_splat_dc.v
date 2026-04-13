// Test don't-care splat: (let a, _...) = (1, 2, 3)
// Only a is bound, rest is discarded.
main(): none
{
  let t = (i32 7, i32 99, i32 100);
  (let a, _...) = t;
  ffi::exit_code a
}
