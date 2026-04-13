// Test type inference backward through CallDyn.
// When a static Call expects type T for an arg that came from
// a CallDyn (arithmetic) on default-typed literals, the literals
// should be refined to T.

wrapper
{
  val: i32;

  create(val: i32): wrapper
  {
    new {val}
  }
}

main(): none
{
  var result = 0;

  // i32(1 + 2): the + is a CallDyn on default u64 literals.
  // The i32::create call expects i32, so 1 and 2 should be
  // backward-refined to i32, making the + resolve to i32::+.
  let a = wrapper(i32(1 + 2));

  if a.val == i32 3
  {
    result = result + 1;
  }

  // i32(10 - 3): similar backward refinement through subtraction.
  let b = wrapper(i32(10 - 3));

  if b.val == i32 7
  {
    result = result + 2;
  }

  // result should be 3 (both tests pass).
  ffi::exit_code result
}
