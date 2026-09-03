// Tests: an omitted argument on an explicitly named enclosing class in a call
// path becomes an inference hole and is recovered from the active generic
// instantiation.
// Failure mode: `inner::callee` loses `C`, remains unresolved, or reifies with
// a type unrelated to the enclosing `inner[i32]`.
// Assumptions: call paths may contain inference holes, and the caller and
// callee are methods of the same instantiated generic class.

inner[C]
{
  callee(c: C): C
  {
    c
  }

  caller(): u64
  {
    inner::callee(4)
  }
}

main(): none
{
  var result = 0;

  if inner[i32]::caller() != 4
  {
    result = result + 1
  }

  ffi::exit_code result
}
