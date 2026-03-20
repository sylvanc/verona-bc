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

  bool(self: string): bool
  {
    self.size != 0
  }

  ==(self: string, other: string): bool
  {
    if self.size != other.size
    {
      return false
    }

    self.data.compare(0, other.data, 0, self.size) == i64 0
  }

  !=(self: string, other: string): bool
  {
    !(self == other)
  }

  <(self: string, other: string): bool
  {
    let len = self.size.min(other.size);
    let cmp = self.data.compare(0, other.data, 0, len);

    if cmp < i64 0
    {
      return true
    }
    else if cmp > i64 0
    {
      return false
    }

    self.size < other.size
  }

  <=(self: string, other: string): bool
  {
    !(other < self)
  }

  >(self: string, other: string): bool
  {
    other < self
  }

  >=(self: string, other: string): bool
  {
    !(self < other)
  }

  +(self: string, other: string): string
  {
    let result = array[u8]::fill(self.size + other.size);
    result.copy_from(0, self.data, 0, self.size);
    result.copy_from(self.size, other.data, 0, other.size);
    string(result)
  }
}
