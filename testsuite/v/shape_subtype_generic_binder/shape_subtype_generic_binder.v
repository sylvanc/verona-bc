// Tests: FlatClass shape subtyping preserves class- and function-level generic
// binders, including shape-driven parameter and return inference for generated
// lambdas under multiple enclosing class instantiations.
// Failure mode: inference diverges by growing a union, reification reports a
// missing or ambiguous binder, the compiler crashes on a detached TypeParam,
// or a specialized call returns the wrong value.
// Assumptions: `box[B]` structurally satisfies `getter[B]`; generated lambdas
// mirror enclosing binders and must be retargeted to their local canonical
// TypeParam before reification.

shape getter[T]
{
  get(self: self): T;
}

shape unop[T]
{
  apply(self: self, x: T): T;
}

box[B]
{
  v: B;

  create(v: B): box[B]
  {
    new {v}
  }

  get(self: box[B]): B
  {
    self.v
  }
}

wrap[B]
{
  extract(g: getter[B]): B
  {
    g.get
  }

  run(b: B): B
  {
    let x = box[B](b);
    wrap[B]::extract(x)
  }
}

extract_function[T](g: getter[T]): T
{
  g.get
}

run_function[B](b: B): B
{
  let x = box[B](b);
  shape_subtype_generic_binder::extract_function[B](x)
}

call_function_lambda[T](f: unop[T], x: T): T
{
  f(x)
}

run_function_lambda[B](x: B): B
{
  let f = (y) -> { y };
  shape_subtype_generic_binder::call_function_lambda[B](f, x)
}

lambda_wrap[B]
{
  marker: i32;

  create(marker: i32): lambda_wrap[B]
  {
    new {marker}
  }

  call(f: unop[B], x: B): B
  {
    f(x)
  }

  run(self: lambda_wrap[B], x: B): B
  {
    let f = (y) -> { y };
    lambda_wrap[B]::call(f, x)
  }
}

main(): none
{
  var result = 0;

  if wrap[i32]::run(21) != 21 { result = result + 1 }
  if shape_subtype_generic_binder::run_function[i32](42) != 42
  {
    result = result + 2
  }
  let i = lambda_wrap[i32](0);
  if i.run(5) != 5 { result = result + 4 }
  let u = lambda_wrap[u64](0);
  if u.run(6) != 6 { result = result + 8 }
  if shape_subtype_generic_binder::run_function_lambda[i32](7) != 7
  {
    result = result + 16
  }

  ffi::exit_code result
}
