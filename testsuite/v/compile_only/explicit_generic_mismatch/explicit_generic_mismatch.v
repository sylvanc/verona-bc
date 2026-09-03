// Tests: explicitly different instantiations of the same generic class are
// rejected when a function requires one specific instantiation.
// Failure mode: compilation succeeds by erasing or conflating the `i32` and
// `u64` type arguments.
// Assumptions: generic class arguments are invariant and constructor inference
// preserves the explicitly supplied `wrapper[u64]` argument.

wrapper[T]
{
  value: T;

  create(value: T): wrapper[T]
  {
    new {value}
  }
}

accept_i32(value: wrapper[i32]): none {}

main(): none
{
  explicit_generic_mismatch::accept_i32(wrapper[u64](42));
}
