// Test splat in middle: (let a, let rest..., let d) = (1, 2, 3, 4)
// rest should be a tuple (2, 3).
main(): i32
{
  let t = (i32 1, i32 2, i32 3, i32 4);
  (let a, let rest..., let d) = t;
  // rest is (i32, i32), destructure it.
  (let b, let c) = rest;
  // a=1, b=2, c=3, d=4, sum = 10
  a + b + c + d
}
