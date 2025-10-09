// This is a bag of bytes, not a UTF-8 string. It can contain nulls, and is
// always null-terminated.
string
{
  // Underlying storage as bytes.
  bytes: array[u8];
  size: usize;

  // Create a string from an existing array of bytes.
  create(bytes: array[u8] = array[u8](usize 0), size: usize = -1.usize): string
  {
    // TODO: passed in size?
    let len = bytes.size;
    var i = 0.usize;

    while i < len
    {
      if bytes(i) == 0.u8
      {
        break
      }

      i = i + 1.usize
    }

    new (bytes, i)
  }

  // Indexing returns a ref[u8] so callers can read/write bytes.
  ref apply(self: string, index: usize): ref[u8]
  {
    self.bytes(index)
  }
}
