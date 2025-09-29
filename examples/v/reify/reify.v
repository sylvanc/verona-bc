range[A = i32]
{
  curr: A;
  stop: A;
  step: A;

  create(curr: A, stop: A, step: A = A(1)): range[A]
  {
    // TODO: check curr <= stop if step > 0, curr >= stop if step < 0
    new (curr, stop, step)
  }

  has_next(self: range[A]): bool
  {
    if (self.step < A(0))
    {
      self.curr > self.stop
    }
    else
    {
      self.curr < self.stop
    }
  }

  next(self: range[A]): A
  {
    self.curr = self.curr + self.step
  }
}

main(): i32
{
  let a = array[i32](10.usize);
  let r = range(i32 0, i32 10);
  var sum = i32 0;

  for r v ->
  {
    sum = sum + v;
  }

  let a0 = ref a(usize 0);
  *a0 = sum;
  let x = *a0;

  let cownx = when ()
  {
    x
  }

  let cownx2 = when (cownx) y ->
  {
    y * 2
  }

  let cownx3 = when (cownx, cownx2) (y, z) ->
  {
    y + z
  }

  x
}
