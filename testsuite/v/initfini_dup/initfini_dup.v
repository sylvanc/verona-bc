// Test that duplicate init/fini functions for the same library are rejected.
// Both use blocks declare the same library (default) and both have init().

use
{
  init(): none {}
  printval = "printval"(any): none;
}

use
{
  init(): none {}
  printval = "printval"(any): none;
}

main(): i32
{
  :::printval(0);
  0
}
