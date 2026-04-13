// Test: shape-aware TypeArg inference.
// When a function takes getter[T] (a shape), calling with box[i32]
// (a concrete class conforming to the shape) should infer T=i32 by
// matching the shape's method return types against the class's.
shape getter[T]
{
  get(self: self): T;
}

box[T]
{
  val: T;

  create(val: T): box[T]
  {
    new {val}
  }

  get(self: box[T]): T
  {
    self.val
  }
}

extract[T](g: getter[T]): T
{
  g.get
}

main(): none
{
  let b = box[i32](42);
  ffi::exit_code(infer_shape::extract(b))
}
