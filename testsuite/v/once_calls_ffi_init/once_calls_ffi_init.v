// FFI initialization must run before a once function that uses the library.
// The initializer prints 1, the once body prints 2, and main then prints 3.
// Missing the FFI dependency or ordering it late changes the observed sequence.
// Expected stdout: 1, 2, 3.

once run(): none
{
  :::printval(2);
}

use
{
  init(): any
  {
    :::init_printval(1);
  }

  init_printval = "printval"(any): none;
  printval = "printval"(any): none;
}

main(): none
{
  once_calls_ffi_init::run();
  :::printval(3);
}
