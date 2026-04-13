// Test: structured TypeArg inference.
// When a function takes wrapper[T], calling with wrapper[i32]
// should infer T=i32.
wrapper[T]
{
  val: T;

  create(val: T): wrapper[T]
  {
    new {val}
  }
}

unwrap[T](w: wrapper[T]): T
{
  w.val
}

main(): none
{
  let w = wrapper[i32](1);
  ffi::exit_code(infer_struct::unwrap(w))
}
