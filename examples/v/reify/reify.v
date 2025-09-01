range[A = i32]
{
  curr: A;
  high: A;
  step: A;

  create(curr: A, high: A, step: A = A(1))
  {
    new (curr, high, step)
  }

  has_next(self: range[A]): bool
  {
    self.curr < self.high
  }

  next(self: range[A]): A
  {
    self.curr = self.curr + self.step
  }
}

main(): i32
{
  let r = range(0, 10);
  var sum = i32 0;

  while r.has_next
  {
    sum = sum + r.next
  }

  sum
}
