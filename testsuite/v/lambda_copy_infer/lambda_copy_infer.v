// Test that TypeVar inference propagates through Copy/Move aliases.
// The lambda param x is copied to y, and y is used in the call.
// After inference, x should also get the inferred type.
wrapper[T]
{
  val: T;

  create(val: T): wrapper[T]
  {
    new { val = val }
  }

  get(self): T
  {
    self.val
  }
}

wrap_copy[T](val: T): wrapper[T]
{
  let f = (x) -> { let y = x; wrapper[T](y) };
  f(val)
}

main(): i32
{
  let w = wrap_copy[i32](42);
  w.get
}
