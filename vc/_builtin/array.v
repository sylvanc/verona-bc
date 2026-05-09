array[T]
{
  fill(size: usize, from: T = T): array[T]
  {
    let a = :::newarray[T](size);
    a.fill_range(0, size, from);
    a
  }

  // Allocate `size` elements. Storage is zero-initialized: primitive
  // element types are written with memset(0), so reads return 0 / 0.0
  // / false until the caller assigns. Reference element types are
  // initialized to `none`. This is safe to read-before-write for any
  // primitive T or any T whose default `none` is acceptable; otherwise
  // the caller must populate before reading.
  alloc(size: usize): array[T]
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

  each(self: array[T], f: (usize, T) -> none): none
  {
    var i: usize = 0;

    while i < self.size
    {
      f(i, self(i));
      i = i + 1
    }
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

  // Materialize an `each`-style source into a fresh array.
  // Two-pass: count via each, then allocate, then fill via each.
  // Lambdas use explicit `: none` to discard their body value at the
  // shape boundary (the convention used throughout algo for lambdas
  // in generic functions whose receiver is a typeparam).
  create[Src, K](src: Src): array[T]
  {
    var n: usize = 0;
    src.each (k: K, x: T): none -> { n = n + 1; }
    let a: array[T] = array[T]::alloc(n);
    var i: usize = 0;
    src.each (k: K, x: T): none -> { a(i) = x; i = i + 1; }
    a
  }
}
