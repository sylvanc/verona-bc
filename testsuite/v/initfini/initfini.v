// Test that init runs before main and init's returned lambda runs after main.
// Expected stdout: 1, 2, 3 (init prints 1, main prints 2, fini lambda prints 3).

use
{
  init(): any
  {
    var x: i32 = 1;
    :::printval(x);
    let y: i32 = 3;
    { :::printval(y); }
  }

  printval = "printval"(any): none;
}

main(): i32
{
  var x: i32 = 2;
  :::printval(x);
  0
}
