// Test finalizer support via `final` methods.
// Stack-allocated objects should have their finalizer called
// when the enclosing function returns, observing the final field state.

use
{
  printval = "printval"(any): none;
}

resource
{
  val: i32;

  final(self: resource)
  {
    :::printval(self.val)
  }
}

main(): i32
{
  var a = resource(10);
  a.val = 42;
  0
}
