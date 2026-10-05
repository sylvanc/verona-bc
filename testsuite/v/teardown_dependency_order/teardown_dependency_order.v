// A teardown callback that uses another initialized library keeps that
// library alive until the callback has completed. A's callback touches B, so A
// must tear down before B. Missing that lifetime edge prints B's final 5 before
// A's 0 and 4, causing the stdout golden to fail.
// Expected stdout: 1, 2, 0, 3, 0, 4, 5.

library_b
{
  touch(): none
  {
    :::touch_b(0);
  }

  use
  {
    init(): any
    {
      :::print_b(1);
      { :::print_b(5); }
    }

    print_b = "printval"(any): none;
    touch_b = "printval"(any): none;
  }
}

library_a
{
  finish(): none
  {
    library_b::touch();
    :::print_a(4);
  }

  start(): none
  {
    :::touch_a(0);
  }

  use
  {
    init(): any
    {
      :::print_a(2);
      { library_a::finish(); }
    }

    print_a = "printval"(any): none;
    touch_a = "printval"(any): none;
  }
}

main(): none
{
  library_a::start();
  :::printval(3);
}

use
{
  printval = "printval"(any): none;
}
