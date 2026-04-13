// Test unary minus on a default integer refining from signed context.
// Both a typed local and a function return should push `-1` to `i64`.

neg(): i64
{
  -1
}

main(): none
{
  var result = 0;

  let x: i64 = -1;
  if !(x == (i64 0 - i64 1)) { result = result + 1 }

  if !(infer_neg_defaultint::neg() == (i64 0 - i64 1)) { result = result + 2 }

  ffi::exit_code result
}
