string
{
  data: array[u8];
  len: usize;

  // --- Core ---

  create(data: array[u8]): string
  {
    new { data, len = data.size - 1 }
  }

  copy(self: string): string
  {
    let d = array[u8]::fill(self.len + 1);
    d.copy_from(0, self.data, 0, self.len + 1);
    new { data = d, len = self.len }
  }

  size(self: string): usize
  {
    self.len
  }

  ref apply(self: string, index: usize): ref[u8]
  {
    ref self.data()(index)
  }

  bool(self: string): bool
  {
    self.len != 0
  }

  cstring(self: string): array[u8]
  {
    self.data
  }

  // --- Comparison ---

  ==(self: string, other: string): bool
  {
    if self.len != other.len
    {
      return false
    }

    self.data.compare(0, other.data, 0, self.len) == 0
  }

  !=(self: string, other: string): bool
  {
    !(self == other)
  }

  <(self: string, other: string): bool
  {
    let n = self.len.min(other.len);
    let cmp = self.data.compare(0, other.data, 0, n);

    if cmp < 0
    {
      return true
    }
    else if cmp > 0
    {
      return false
    }

    self.len < other.len
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

  // --- Concatenation ---

  +(self: string, other: string): string
  {
    let result = array[u8]::fill(self.len + other.len + 1);
    result.copy_from(0, self.data, 0, self.len);
    result.copy_from(self.len, other.data, 0, other.len);
    result(self.len + other.len) = 0;
    string(result)
  }

  // --- Capacity ---

  reserve(self: string, n: usize): none
  {
    if n + 1 > self.data.size
    {
      var cap = self.data.size;
      while cap < (n + 1)
      {
        cap = cap * 2
      }

      let d = array[u8]::fill(cap);
      d.copy_from(0, self.data, 0, self.len + 1);
      self.data = d
    }
  }

  clear(self: string): none
  {
    self.len = 0;
    self.data()(0) = 0
  }

  // --- Mutation ---

  append(self: string, other: string): none
  {
    self.reserve(self.len + other.len);
    self.data.copy_from(self.len, other.data, 0, other.len);
    self.len = self.len + other.len;
    self.data()(self.len) = 0
  }

  insert(self: string, index: usize, other: string): none
  {
    self.reserve(self.len + other.len);
    // shift tail right using bulk copy (handles overlap)
    self.data.copy_from(index + other.len, self.data, index, self.len - index);
    self.data.copy_from(index, other.data, 0, other.len);
    self.len = self.len + other.len;
    self.data()(self.len) = 0
  }

  erase(self: string, offset: usize, count: usize): none
  {
    let n = count.min(self.len - offset);
    // shift tail left using bulk copy (handles overlap)
    self.data.copy_from(offset, self.data, offset + n, self.len - offset - n);
    self.len = self.len - n;
    self.data()(self.len) = 0
  }

  replace(self: string, offset: usize, count: usize, other: string): none
  {
    let n = count.min(self.len - offset);
    let new_len = self.len - n + other.len;
    self.reserve(new_len);
    // shift tail to final position, then copy replacement in
    self.data.copy_from(offset + other.len, self.data, offset + n, self.len - offset - n);
    self.data.copy_from(offset, other.data, 0, other.len);
    self.len = new_len;
    self.data()(self.len) = 0
  }

  // --- Search ---

  find(self: string, other: string): usize | none
  {
    if other.len == 0
    {
      return usize 0
    }

    if other.len > self.len
    {
      return none
    }

    var i: usize = 0;
    let limit = self.len - other.len + 1;

    while i < limit
    {
      if self.data.compare(i, other.data, 0, other.len) == 0
      {
        let found: usize = i;
        return found
      }
      i = i + 1
    }

    none
  }

  contains(self: string, other: string): bool
  {
    match self.find(other)
    {
      (i: usize) -> true;
    }
    else
    {
      false
    }
  }

  starts_with(self: string, prefix: string): bool
  {
    if prefix.len > self.len
    {
      return false
    }

    self.data.compare(0, prefix.data, 0, prefix.len) == 0
  }

  ends_with(self: string, suffix: string): bool
  {
    if suffix.len > self.len
    {
      return false
    }

    let offset = self.len - suffix.len;
    self.data.compare(offset, suffix.data, 0, suffix.len) == 0
  }

  // --- Slicing ---

  substring(self: string, offset: usize, count: usize): string
  {
    let n = count.min(self.len - offset);
    let result = array[u8]::fill(n + 1);
    result.copy_from(0, self.data, offset, n);
    result(n) = 0;
    string(result)
  }

  // --- Trimming ---

  is_space(c: u8): bool
  {
    (c == 32) | (c == 9) | (c == 10) | (c == 13)
  }

  trim_left(self: string): string
  {
    self.data.pairs (i, c) ->
    {
      if !string::is_space(c)
      {
        raise self.substring(i, self.len - i)
      }
    }

    string(array[u8]::fill(1))
  }

  trim_right(self: string): string
  {
    var i = self.len;
    while i > 0
    {
      if !string::is_space(self.data()(i - 1))
      {
        return self.substring(0, i)
      }
      i = i - 1
    }

    string(array[u8]::fill(1))
  }

  trim(self: string): string
  {
    var start: usize = 0;
    while start < self.len
    {
      let s: usize = start;
      if !string::is_space(self.data()(s))
      {
        var end = self.len;
        while end > s
        {
          if !string::is_space(self.data()(end - 1))
          {
            return self.substring(s, end - s)
          }
          end = end - 1
        }

        return self.substring(s, 0)
      }
      start = start + 1
    }

    string(array[u8]::fill(1))
  }
}
