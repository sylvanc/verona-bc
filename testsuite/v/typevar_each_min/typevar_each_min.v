each_last[T, U](c: T): U | none
{
  var best: U | none = none;
  c.each (x: U): none -> {
    best = x
  };
  best
}

main(): none
{
  let a = array[i32]::fill(3);
  a(0) = 5;
  a(1) = 3;
  a(2) = 8;
  match typevar_each_min::each_last(a)
  {
    (n: i32) -> ffi::exit_code n
  }
  else { ffi::exit_code 99 }
}
