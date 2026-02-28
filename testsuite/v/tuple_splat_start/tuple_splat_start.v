// Test splat at start: (let rest..., let c) = (1, 2, 3)
// rest should be a tuple (1, 2).
main(): i32
{
  let t = (i32 1, i32 2, i32 3);
  (let rest..., let c) = t;
  // rest is (i32, i32), destructure it.
  (let a, let b) = rest;
  // a=1, b=2, c=3, sum = 6
  a + b + c
}
