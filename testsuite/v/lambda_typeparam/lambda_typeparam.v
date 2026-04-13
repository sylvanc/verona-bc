// Test: a lambda inside a generic function that uses the
// function's type parameter. The lambda class must capture
// free type parameter references as its own type parameters,
// with appropriate type arguments at the creation site.
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
  let f = (x: T): wrapper[T] -> { wrapper[T](x) }
  f(val)
}

main(): none
{
  let w = lambda_typeparam::apply_and_wrap[i32](42);
  ffi::exit_code(w.get)
}
