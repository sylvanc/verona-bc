// Ordinary recursive helpers reached by a once initializer are legal. helper
// and other form a recursive call component, but the value-zero path returns
// 42. Mistaking ordinary recursion for a once cycle rejects compilation.
// Expected exit code: 0.

helper(value: i32): i32
{
  if value == 0
  {
    42
  }
  else
  {
    memo_ordinary_recursion::other(value - 1)
  }
}

other(value: i32): i32
{
  memo_ordinary_recursion::helper(value)
}

once answer(): i32
{
  memo_ordinary_recursion::helper(0)
}

main(): none
{
  let result = memo_ordinary_recursion::answer();
  ffi::exit_code(if result == 42 { 0 } else { 1 })
}
