// Test that register_external_notify compiles and can be called from init.
// The notify callback won't fire since no cowns are used, but verifies
// the plumbing works end-to-end.

use
{
  init(): any
  {
    ffi::register_external_notify((): none -> { :::printval(0) });
    none
  }

  printval = "printval"(any): none;
}

main(): i32
{
  ffi::external.add;
  ffi::external.remove;
  var x: i32 = 0;
  :::printval(x);
  0
}
