array[T]
{
  create(some: array[T]): array[T]
  {
    some
  }

  fill(size: usize, from: T = T): array[T]
  {
    let a = :::newarray[T](size);
    a.fill_range(0, size, from);
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
  }

  pairs(self: array[T], f: (usize, T) -> none): none
  {
    var i = 0;

    while i < self.size
    {
      f(i, self(i));
      i = i + 1
    }
  }

  values(self: array[T]): arrayiter[T]
  {
    arrayiter[T](self)
  }

  copy(self: array[T], offset: usize, len: usize): array[T]
  {
    let n = (self.size - offset) min len;
    let a = :::newarray[T](n);
    a.copy_from(0, self, offset, n);
    a
  }

  copy_from(self: array[T], dst_offset: usize,
    src: array[T], src_offset: usize, len: usize): none
  {
    :::arraycopy(self, dst_offset, src, src_offset, len)
  }

  fill_range(self: array[T], offset: usize, len: usize, value: T): none
  {
    :::arrayfill(self, offset, len, value)
  }

  compare(self: array[T], self_offset: usize,
    other: array[T], other_offset: usize, len: usize): i64
  {
    :::arraycmp(self, self_offset, other, other_offset, len)
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
