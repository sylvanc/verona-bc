use "./tempgit" "main";

// range[A = i32]
// {
//   curr: A;
//   stop: A;
//   step: A;

//   create(curr: A, stop: A, step: A = A(1)): range[A]
//   {
//     // TODO: check curr <= stop if step > 0, curr >= stop if step < 0
//     new (curr, stop, step)
//   }

//   has_next(self: range[A]): bool
//   {
//     if (self.step < A(0))
//     {
//       self.curr > self.stop
//     }
//     else
//     {
//       self.curr < self.stop
//     }
//   }

//   next(self: range[A]): A
//   {
//     self.curr = self.curr + self.step
//   }
// }

use
{
  printval = "printval"(any): none;
}

main(): i32
{
  let a = array[i32](10.usize);
  let r = range(0.i32, 10.i32);
  var sum = 0.i32;

  for r v ->
  {
    sum = sum + v;
  }

  let a0 = ref a(0.usize);
  *a0 = sum;
  let x = *a0;

  let cownx = when ()
  {
    :::printval(x);
    x
  }

  let cownx2 = when cownx x ->
  {
    let r = *x * 2.i32;
    :::printval(r);
    r
  }

  let cownx3 = when (cownx.read, cownx2.read) (x, x2) ->
  {
    let r = *x + *x2;
    :::printval(r);
    r
  }

  x
}
