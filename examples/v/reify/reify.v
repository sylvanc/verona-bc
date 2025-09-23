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

// use cycle1 = cycle2;
// use cycle2 = cycle1;

main(): i32
{
  // let f = cycle1;
  let a = array[i32](usize 10);
  let r = range(i32 0, i32 10);
  var sum = i32 0;

  for r v ->
  {
    sum = sum + v;
  }

  let a0 = ref a(usize 0);
  *a0 = sum;
  a(usize 0)
}
