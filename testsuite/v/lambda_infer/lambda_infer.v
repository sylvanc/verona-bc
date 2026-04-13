// Test: lambda with unspecified parameter and return types.
// Types should be inferred from usage in the lambda body.

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

apply_and_wrap[T](val: T): wrapper[T]
{
  let f = (x) -> { wrapper[T](x) }
  f(val)
}

main(): none
{
  let w = lambda_infer::apply_and_wrap[i32](42);
  ffi::exit_code(w.get)
}
