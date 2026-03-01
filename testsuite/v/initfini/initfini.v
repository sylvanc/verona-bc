// Test that init runs before main and fini runs after main.
// Expected stdout: 1, 2, 3 (init prints 1, main prints 2, fini prints 3).

use
{
  init(): none
  {
    var x: i32 = 1;
    :::printval(x);
  }

  fini(): none
  {
    var x: i32 = 3;
    :::printval(x);
  }

  printval = "printval"(any): none;
}

main(): i32
{
  var x: i32 = 2;
  :::printval(x);
  0
}
