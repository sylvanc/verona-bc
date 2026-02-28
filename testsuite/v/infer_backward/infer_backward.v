// Test: backward refinement of default-inferred TypeArgs.
// wrap(42) initially infers T=u64 from the default literal type.
// unwrap_i32(w) expects wrapper[i32], triggering backward refinement
// of wrap's TypeArgs from [u64] to [i32], and refinement of the
// literal 42 from u64 to i32.
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

wrap[T](val: T): wrapper[T]
{
  wrapper(val)
}

unwrap_i32(w: wrapper[i32]): i32
{
  w.get
}

main(): i32
{
  let w = infer_backward::wrap(42);
  infer_backward::unwrap_i32(w)
}
