// Test finalizer support via `final` methods.
// Stack-allocated objects should have their finalizer called
// when the enclosing function returns.

use
{
  printval = "printval"(any): none;
}

resource
{
  val: i32;

  final(self: resource)
  {
    // Print the value during finalization.
    :::printval(self.val)
  }
}

main(): i32
{
  var result = 0;

  // Test: create two resources, verify they print during finalization.
  let a = resource(10);
  let b = resource(20);

  // The finalizers run when the function returns.
  // We can't check return values from finalizers, but we verify
  // the program compiles and runs without errors.
  // Exit code 0 means success.
  result
}
