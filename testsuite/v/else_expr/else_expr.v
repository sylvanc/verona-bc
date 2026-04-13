maybe_val(flag: bool): i32 | nomatch
{
  if flag { i32 42 }
}

main(): none
{
  let a: i32 = else_expr::maybe_val(true) else (i32 0);
  let b: i32 = else_expr::maybe_val(false) else (i32 0);

  var result: i32 = 0;

  if !(a == 42)
  {
    result = result + 1
  }

  if !(b == 0)
  {
    result = result + 2
  }

  ffi::exit_code result
}
