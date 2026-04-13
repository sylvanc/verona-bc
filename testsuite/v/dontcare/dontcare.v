// Test using _ (DontCare) on the LHS of assignments.
main(): none
{
  var result = 0;

  // Simple discard assignment.
  _ = i32 42;

  // Tuple destructuring: ignore first, keep second.
  let t1 = (i32 10, i32 20);
  (_, let b1) = t1;
  if !(b1 == 20) { result = result + 1 }

  // Tuple destructuring: keep first, ignore second.
  let t2 = (i32 30, i32 40);
  (let a2, _) = t2;
  if !(a2 == 30) { result = result + 2 }

  // Ignore both elements.
  let t3 = (i32 50, i32 60);
  (_, _) = t3;

  // Var + discard.
  var x = i32 0;
  let t4 = (i32 70, i32 80);
  (x, _) = t4;
  if !(x == 70) { result = result + 4 }

  ffi::exit_code result
}
