// Test that an FFI init function can call a once function.
// Expected stdout: 42, 0 (init prints the memoized value before main runs).

once answer(): i32
{
  42
}

use
{
  init(): any
  {
    :::printval(init_calls_once::answer());
  }

  printval = "printval"(any): none;
}

main(): i32
{
  :::printval(0);
  0
}
