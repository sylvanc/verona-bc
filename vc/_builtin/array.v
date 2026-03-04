array[T]
{
  create(some: array[T]): array[T]
  {
    some
  }

  fill(size: usize, from: T = T): array[T]
  {
    let a = :::newarray[T](size);
    var i = 0;

    while i < size
    {
      a(i) = from;
      i = i + 1
    }

    a
  }

  size(self: array[T]): usize
  {
    :::len(self)
  }

  ref apply(self: array[T], index: usize): ref[T]
  {
    :::arrayref(self, index)
  }

  each(self: array[T], f: T -> none): none
  {
    var i = 0;

    while i < self.size
    {
      f(self(i));
      i = i + 1
    }

    none
  }

  pairs(self: array[T], f: (usize, T) -> none): none
  {
    var i = 0;

    while i < self.size
    {
      f(i, self(i));
      i = i + 1
    }

    none
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

  rewind(self: arrayiter[T])
  {
    self.index = 0
  }
}
