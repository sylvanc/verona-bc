array[T]
{
  create(size: usize): array[T]
  {
    :::newarray[T](size)
  }

  size(self: array[T]): usize
  {
    :::len(self)
  }

  ref apply(self: array[T], index: usize): ref[T]
  {
    :::arrayref(self, index)
  }

  values(self: array[T]): arrayiter[T]
  {
    arrayiter[T](self)
  }
}

arrayiter[T]
{
  index: usize;
  arr: array[T];

  create(arr: array[T]): arrayiter[T]
  {
    new { index = 0, arr }
  }

  next(self: arrayiter[T]): T | nomatch
  {
    if self.index < self.arr.size
    {
      let a = self.arr;
      let item = a(self.index);
      self.index = self.index + 1;
      item
    }
    else
    {
      nomatch
    }
  }
}
