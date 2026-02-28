// Test splat at end: (let a, let rest...) = (1, 2, 3)
// rest should be a tuple (2, 3).
main(): i32
{
  let t = (i32 1, i32 2, i32 3);
  (let a, let rest...) = t;
  // rest is (i32, i32), destructure it.
  (let b, let c) = rest;
  // a=1, b=2, c=3, bitpattern: a*4 + b*2 + c = 4+4+3 = 11
  // Actually just return sum: 1+2+3 = 6
  a + b + c
}
