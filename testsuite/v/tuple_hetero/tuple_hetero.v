// Test heterogeneous tuple: different element types.
// TupleType carries per-element type info through the IR.
main(): i32
{
  // Construct a tuple of (i32, i64).
  let t = (i32 3, i64 5);

  // Destructure into typed variables.
  (let a, let b) = t;

  // Return a (i32). a=3, b=5, sum=8.
  a + b.i32
}
