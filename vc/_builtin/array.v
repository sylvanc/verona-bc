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
}
