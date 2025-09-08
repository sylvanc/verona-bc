range[A = i32]
{
  curr: A;
  stop: A;
  step: A;

  create(curr: A, stop: A, step: A = A(1))
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

id[A](x: A): A
{
  x
}

main(): i32
{
  let r = range(0, 10);
  var sum = i32 0;

  while r.has_next
  {
    sum = sum + r.next
  }

  id[i32](sum)
}
