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

    var i = 0;

    while i < self.size
    {
      if (self.data)(i) != (other.data)(i)
      {
        return false
      }

      i = i + 1
    }

    true
  }

  !=(self: string, other: string): bool
  {
    !(self == other)
  }

  <(self: string, other: string): bool
  {
    let len = self.size.min(other.size);
    var i = 0;

    while i < len
    {
      let sc = (self.data)(i);
      let oc = (other.data)(i);

      if sc < oc
      {
        return true
      }
      else if oc < sc
      {
        return false
      }

      i = i + 1
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
    var i = 0;

    while i < self.size
    {
      result(i) = (self.data)(i);
      i = i + 1
    }

    var j = 0;

    while j < other.size
    {
      result(i) = (other.data)(j);
      i = i + 1;
      j = j + 1
    }

    string(result)
  }
}
