string
{
  // Underlying storage as bytes.
  bytes: array[u8];

  // Create a string from an existing array of bytes.
  create(bytes: array[u8] = array[u8](usize 0)): string
  {
    new bytes
  }

  // Length of the string in bytes.
  size(self: string): usize
  {
    self.bytes.size()
  }

  // Indexing returns a ref[u8] so callers can read/write bytes.
  ref apply(self: string, index: usize): ref[u8]
  {
    self.bytes(index)
  }
}
