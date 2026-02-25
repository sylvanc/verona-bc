string
{
  data: array[u8];

  create(data: array[u8]): string
  {
    new { data }
  }

  size(self: string): usize
  {
    self.data.size
  }

  ref apply(self: string, index: usize): ref[u8]
  {
    ref self.data.apply(index)
  }

  values(self: string): arrayiter[u8]
  {
    self.data.values
  }
}
