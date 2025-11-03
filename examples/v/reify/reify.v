use "https://github.com/sylvanc/iterwise" "main";

use
{
  printval = "printval"(any): none;
}

main(): i32
{
  // let r = range[i32](0.i32, 5.i32) chain[i32]: range[i32](5.i32, 10.i32);
  let r = range[i32] 0.i32 5.i32 chain[i32]: range[i32] 5.i32 10.i32;
  // let r = range 0 5 chain: range 5 10;
  var sum = 0.i32;

  for r v ->
  {
    sum = sum + v;
  }

  :::printval(sum);

  // let x = sum;

  // let cownx = when ()
  // {
  //   :::printval(x);
  //   x
  // }

  // let cownx2 = when cownx x ->
  // {
  //   let r = *x * 2.i32;
  //   :::printval(r);
  //   r
  // }

  // let cownx3 = when (cownx.read, cownx2.read) (x, x2) ->
  // {
  //   let r = *x + *x2;
  //   :::printval(r);
  //   r
  // }

  0.i32
}
