// Test: CallDyn result type tracking with TypeArg inference.
// The result of a method call (CallDyn) should be tracked
// so it can be used for subsequent TypeArg inference.
// Here, w.get returns i32 (T=i32), and identity[T] needs that
// type to infer T.
wrapper[T]
{
  val: T;

  create(val: T): wrapper[T]
  {
    new {val = val}
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

main(): i32
{
  let w = wrapper[i32](1);
  let v = w.get;
  identity(v)
}
