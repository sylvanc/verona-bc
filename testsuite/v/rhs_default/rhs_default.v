// Test: explicit RHS function with default arguments takes precedence
// over auto-generated RHS from an LHS function.
// A class has an lhs `get` function and an explicit rhs `get` with a default
// argument. The default-expanded rhs `get` (arity 1) should be used instead
// of auto-generating one that forwards to the lhs.

myclass
{
  create(): myclass { new {} }
  ref get(self: myclass): i32 { 42.i32 }
  get(self: myclass, offset: i32 = 10.i32): i32 { 42.i32 + offset }
}

main(): i32
{
  let obj = myclass();
  // Calling obj.get() should use the default-expanded rhs: get(self, 10)
  // which returns 42 + 10 = 52, NOT the auto-rhs which would return 42.
  obj.get()
}
