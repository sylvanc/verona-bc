// Tests: constructor sugar preserves explicit class arguments while inferring
// omitted type arguments declared by the selected `create` function.
// Failure mode: `T` or `U` remains unresolved, is inferred from the wrong
// argument, or the constructed object's stored value has the wrong type.
// Assumptions: the ignored arguments constrain only the function-level type
// parameters; `box[i32]` explicitly fixes the class-level parameter.

factory
{
  result: i32;

  create[T](ignored: T): factory
  {
    new {result = 7}
  }
}

box[T]
{
  value: T;

  create[U](value: T, ignored: U): box[T]
  {
    new {value}
  }
}

main(): none
{
  var made = factory(42);
  var boxed = box[i32](9, true);
  var result = 0;

  if made.result != 7
  {
    result = result + 1
  }

  if boxed.value != 9
  {
    result = result + 2
  }

  ffi::exit_code result
}
