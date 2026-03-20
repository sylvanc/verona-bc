string
{
  data: array[u8];
  len: usize;

  // --- Core ---

  create(data: array[u8]): string
  {
    let d = array[u8]::fill(data.size);
    d.copy_from(0, data, 0, data.size);
    new { data = d, len = data.size - 1 }
  }

  size(self: string): usize
  {
    self.len
  }

  ref apply(self: string, index: usize): ref[u8]
  {
    ref (self.data)(index)
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

    self.data.compare(0, other.data, 0, self.len) == i64 0
  }

  !=(self: string, other: string): bool
  {
    !(self == other)
  }

  <(self: string, other: string): bool
  {
    let n = self.len.min(other.len);
    let cmp = self.data.compare(0, other.data, 0, n);

    if cmp < i64 0
    {
      return true
    }
    else if cmp > i64 0
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
    (result)(self.len + other.len) = u8 0;
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
    (self.data)(0) = u8 0
  }

  // --- Mutation ---

  append(self: string, other: string): none
  {
    self.reserve(self.len + other.len);
    self.data.copy_from(self.len, other.data, 0, other.len);
    self.len = self.len + other.len;
    (self.data)(self.len) = u8 0
  }

  insert(self: string, index: usize, other: string): none
  {
    self.reserve(self.len + other.len);
    var i = self.len;
    while i > index
    {
      (self.data)(i + other.len - 1) = (self.data)(i - 1);
      i = i - 1
    }
    self.data.copy_from(index, other.data, 0, other.len);
    self.len = self.len + other.len;
    (self.data)(self.len) = u8 0
  }

  erase(self: string, offset: usize, count: usize): none
  {
    let n = count.min(self.len - offset);
    var i = offset;
    while i + n < self.len
    {
      (self.data)(i) = (self.data)(i + n);
      i = i + 1
    }
    self.len = self.len - n;
    (self.data)(self.len) = u8 0
  }

  replace(self: string, offset: usize, count: usize, other: string): none
  {
    self.erase(offset, count);
    self.insert(offset, other)
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
      return none()
    }

    var i: usize = 0;
    let limit = self.len - other.len + 1;

    while i < limit
    {
      if self.data.compare(i, other.data, 0, other.len) == i64 0
      {
        let found: usize = i;
        return found
      }
      i = i + 1
    }

    none()
  }

  contains(self: string, other: string): bool
  {
    (match self.find(other) { (i: usize) -> true; }) else (false)
  }

  starts_with(self: string, prefix: string): bool
  {
    if prefix.len > self.len
    {
      return false
    }

    self.data.compare(0, prefix.data, 0, prefix.len) == i64 0
  }

  ends_with(self: string, suffix: string): bool
  {
    if suffix.len > self.len
    {
      return false
    }

    let offset = self.len - suffix.len;
    self.data.compare(offset, suffix.data, 0, suffix.len) == i64 0
  }

  // --- Slicing ---

  substring(self: string, offset: usize, count: usize): string
  {
    let n = count.min(self.len - offset);
    let result = array[u8]::fill(n + 1);
    result.copy_from(0, self.data, offset, n);
    (result)(n) = u8 0;
    string(result)
  }

  // --- Trimming ---

  trim_left(self: string): string
  {
    var i: usize = 0;
    while i < self.len
    {
      let idx: usize = i;
      let c = (self.data)(idx);
      if c != 32 { if c != 9 { if c != 10 { if c != 13
      {
        let count = self.len - idx;
        return self.substring(idx, count)
      } } } }
      i = i + 1
    }

    string(array[u8]::fill(1))
  }

  trim_right(self: string): string
  {
    var i = self.len;
    while i > 0
    {
      let c = (self.data)(i - 1);
      if c != 32 { if c != 9 { if c != 10 { if c != 13
      {
        return self.substring(0, i)
      } } } }
      i = i - 1
    }

    string(array[u8]::fill(1))
  }

  trim(self: string): string
  {
    self.trim_left.trim_right
  }
}
