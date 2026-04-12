use
{
  strlen = "strlen"(ffi::ptr): usize;
  memcpy = "memcpy"(ffi::ptr, ffi::ptr, usize): ffi::ptr;
}

shape to_string
{
  string(self: self): string;
}

string
{
  data: array[u8];
  len: usize;

  // --- Core ---

  create(data: array[u8]): string
  {
    new { data, len = data.size - 1 }
  }

  // Create a string from a C string pointer.
  from_cstr(ptr: ffi::ptr): string
  {
    if ptr == ffi::ptr
    {
      return string(array[u8]::fill 1)
    }

    let len = :::strlen(ptr);
    let buf = array[u8]::fill(len + 1);
    :::memcpy(buf, ptr, len + 1);
    string buf
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

  string(self: string): string
  {
    self
  }

  cstring(self: string): array[u8]
  {
    self.data
  }

  // --- Comparison ---

  ==(self: string, other: to_string): bool
  {
    let s = other.string;

    if self.len != s.len
    {
      return false
    }

    self.data.compare(0, s.data, 0, self.len) == 0
  }

  !=(self: string, other: to_string): bool
  {
    !(self == other)
  }

  <(self: string, other: to_string): bool
  {
    let s = other.string;
    let n = self.len.min(s.len);
    let cmp = self.data.compare(0, s.data, 0, n);

    if cmp < 0
    {
      return true
    }
    else if cmp > 0
    {
      return false
    }

    self.len < s.len
  }

  <=(self: string, other: to_string): bool
  {
    !(other.string < self)
  }

  >(self: string, other: to_string): bool
  {
    other.string < self
  }

  >=(self: string, other: to_string): bool
  {
    !(self < other)
  }

  // --- Concatenation ---

  +(self: string, other: to_string): string
  {
    let s = other.string;
    let result = array[u8]::fill(self.len + s.len + 1);
    result.copy_from(0, self.data, 0, self.len);
    result.copy_from(self.len, s.data, 0, s.len);
    result(self.len + s.len) = 0;
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

  append(self: string, other: to_string): none
  {
    let s = other.string;
    self.reserve(self.len + s.len);
    self.data.copy_from(self.len, s.data, 0, s.len);
    self.len = self.len + s.len;
    self.data()(self.len) = 0
  }

  insert(self: string, index: usize, other: to_string): none
  {
    // shift tail right using bulk copy (handles overlap)
    let s = other.string;
    self.reserve(self.len + s.len);
    self.data.copy_from(index + s.len, self.data, index, self.len - index);
    self.data.copy_from(index, s.data, 0, s.len);
    self.len = self.len + s.len;
    self.data()(self.len) = 0
  }

  erase(self: string, offset: usize, count: usize): none
  {
    // shift tail left using bulk copy (handles overlap)
    let n = count.min(self.len - offset);
    self.data.copy_from(offset, self.data, offset + n, self.len - offset - n);
    self.len = self.len - n;
    self.data()(self.len) = 0
  }

  replace(self: string, offset: usize, count: usize, other: to_string): none
  {
    // shift tail to final position, then copy replacement in
    let s = other.string;
    let n = count.min(self.len - offset);
    let new_len = self.len - n + s.len;
    self.reserve(new_len);
    self.data.copy_from(
      offset + s.len, self.data, offset + n, self.len - offset - n);
    self.data.copy_from(offset, s.data, 0, s.len);
    self.len = new_len;
    self.data()(self.len) = 0
  }

  // --- Search ---

  find(self: string, other: to_string): usize | none
  {
    let s = other.string;

    if s.len == 0
    {
      return usize 0
    }

    if s.len > self.len
    {
      return none
    }

    var i = 0;
    let limit = self.len - s.len + 1;

    while i < limit
    {
      if self.data.compare(i, s.data, 0, s.len) == 0
      {
        return i
      }
      i = i + 1
    }

    none
  }

  contains(self: string, other: to_string): bool
  {
    match self.find other
    {
      (i: usize) -> true;
    }
    else
    {
      false
    }
  }

  starts_with(self: string, prefix: to_string): bool
  {
    let p = prefix.string;

    if p.len > self.len
    {
      return false
    }

    self.data.compare(0, p.data, 0, p.len) == 0
  }

  ends_with(self: string, suffix: to_string): bool
  {
    let s = suffix.string;

    if s.len > self.len
    {
      return false
    }

    let offset = self.len - s.len;
    self.data.compare(offset, s.data, 0, s.len) == 0
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
    (c == ' ') | (c == '\t') | (c == '\n') | (c == '\r')
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
    var start = 0;

    while start < self.len
    {
      let s = start;

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
