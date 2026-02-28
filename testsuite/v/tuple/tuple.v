// Test tuple construction and destructuring.
// Tuples carry per-element type information through the IR.
main(): i32
{
  // Construct a tuple of (i32, i32).
  let t = (i32 3, i32 5);

  // Destructure using let variables.
  (let a, let b) = t;

  // a=3, b=5, sum=8
  a + b
}
