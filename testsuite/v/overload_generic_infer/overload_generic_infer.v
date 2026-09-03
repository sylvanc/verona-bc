// Tests: post-ANF overload selection uses value arity and handedness before
// sizing and inferring candidate-specific generic argument slots.
// Failure mode: a call selects the wrong arity, RHS calls prefer `once`, or a
// generic candidate receives the wrong number of inferred type arguments.
// Assumptions: exact RHS handedness wins over once fallback; once is used only
// when no matching RHS definition exists.

overloads
{
  select[T](a: T, b: T): T
  {
    a
  }

  select(a: i32): i32
  {
    a
  }

  reverse(a: i32): i32
  {
    a
  }

  reverse[T](a: T, b: T): T
  {
    a
  }

  once handed(): i32
  {
    7
  }

  handed(): i32
  {
    8
  }

  once fallback(): i32
  {
    9
  }
}

main(): none
{
  var result = 0;

  if overloads::select(i32 1) != 1
  {
    result = result + 1
  }

  if overloads::select(i32 2, i32 3) != 2
  {
    result = result + 2
  }

  if overloads::reverse(i32 4) != 4
  {
    result = result + 4
  }

  if overloads::reverse(i32 5, i32 6) != 5
  {
    result = result + 8
  }

  if overloads::handed() != 8
  {
    result = result + 16
  }

  if overloads::fallback() != 9
  {
    result = result + 32
  }

  ffi::exit_code result
}
