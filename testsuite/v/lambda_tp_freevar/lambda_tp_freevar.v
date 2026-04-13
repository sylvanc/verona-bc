// Lambda with both free type parameters and free variables.
// The lambda captures `offset` as a free variable and `T` as a free type param.

wrapper[T]
{
  value: T;

  create(value: T): wrapper[T]
  {
    new {value}
  }

  get(self: wrapper[T]): T
  {
    self.value
  }
}

apply_with_offset[T](val: T, offset: T): wrapper[T]
{
  // Lambda captures `offset` (free var) and uses `T` (free type param).
  // It ignores `x` and wraps `offset` instead.
  let f = (x: T): wrapper[T] -> { wrapper[T](offset) }
  f(val)
}

main(): none
{
  let w = lambda_tp_freevar::apply_with_offset[i32](10, 42);
  ffi::exit_code(w.get)
}
