// An FFI initializer may call its own library through ordinary helpers.
// The initializer prints 1 through helper, then main prints 2. Treating the
// reflexive FFI edge as a cycle rejects compilation; bad ordering changes output.
// Expected stdout: 1, 2.

helper
{
  initialize(): none
  {
    :::init_printval(1);
  }
}

use
{
  init(): any
  {
    helper::initialize();
  }

  init_printval = "printval"(any): none;
  printval = "printval"(any): none;
}

main(): none
{
  :::printval(2);
}
