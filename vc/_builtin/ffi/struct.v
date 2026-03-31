use
{
  malloc = "malloc"(usize): ffi::ptr;
  free = "free"(ffi::ptr): none;
}

struct[A]
{
  size: usize;
  offsets: array[usize];
  kinds: array[u8];

  once create(): struct[A]
  {
    (let size, let offsets, let kinds) = :::ffistruct[A]();
    new {size, offsets, kinds}
  }

  load[B](self: struct[A], from: ffi::ptr, index: usize): B
  {
    :::ffiload[B](from, (self.offsets)(index), (self.kinds)(index))
  }

  store[B](self: struct[A], to: ffi::ptr, index: usize, value: B): none
  {
    :::ffistore[B](to, (self.offsets)(index), (self.kinds)(index), value)
  }

  alloc(self: struct[A]): ffi::ptr
  {
    :::malloc(self.size)
  }

  free(self: struct[A], ptr: ffi::ptr): none
  {
    :::free(ptr)
  }
}
