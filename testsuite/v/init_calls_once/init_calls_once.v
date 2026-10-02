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
    :::init_printval(init_calls_once::answer());
  }

  init_printval = "printval"(any): none;
  printval = "printval"(any): none;
}

main(): none
{
  :::printval(0);
}
