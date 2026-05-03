// Test error: splat let cannot have a type annotation.
main(): none
{
  var t = (1, 2, 3);
  (let a, let rest: i32...) = t;
  a
}
