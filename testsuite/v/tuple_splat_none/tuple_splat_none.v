// Test splat with 0 remaining elements: result is none.
// (let a, let b, let rest...) = (1, 2) → rest = none
main(): i32
{
  let t = (i32 3, i32 5);
  (let a, let b, let rest...) = t;
  // rest is none, a=3, b=5.
  a + b
}
