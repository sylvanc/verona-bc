// Test returning a tuple of lambdas from a function.
make(): (() -> i32, () -> i32)
{
  let a = (): i32 -> 1;
  let b = (): i32 -> 2;
  (a, b)
}

main(): none
{
  (let a, let b) = tuple_lambda::make();
  ffi::exit_code(a() + b() - 3)
}
