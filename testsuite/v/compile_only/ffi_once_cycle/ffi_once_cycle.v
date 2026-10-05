// An FFI initializer and a once function must not depend on each other. The
// initializer calls the once function, which calls back into the FFI library.
// The compiler must reject the cycle; missing either edge would let it compile.

once run(): none
{
  :::printval(1);
}

use
{
  init(): any
  {
    ffi_once_cycle::run();
  }

  printval = "printval"(any): none;
}

main(): none
{
  :::printval(0);
}
