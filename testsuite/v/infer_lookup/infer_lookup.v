// Test: Lookup result type tracking.
// After looking up a method on a typed receiver, the result
// type should be tracked in the type environment so subsequent
// TypeArg inference can use it.
wrapper[T]
{
  val: T;

  create(val: T): wrapper[T]
  {
    new {val}
  }

  get(self: wrapper[T]): T
  {
    self.val
  }
}

identity[T](x: T): T
{
  x
}

main(): none
{
  let w = wrapper[i32](1);
  let v = w.get;
  ffi::exit_code(infer_lookup::identity(v))
}
