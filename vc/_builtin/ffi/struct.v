// use
// {
//   malloc = "malloc"(usize): ffi::ptr;
//   free = "free"(ffi::ptr): none;
// }

// struct[A]
// {
//   size: usize;
//   offsets: array[usize];
//   sizes: array[usize];

//   once create(): struct[A]
//   {
//     (let size, let offsets, let sizes) = :::ffistruct[A]();
//     new {size, offsets, sizes}
//   }

//   load[B](self: struct, from: ffi::ptr, index: usize): B
//   {
//     :::ffiload[B](from, self.offsets()(index), self.sizes()(index))
//   }

//   store[B](self: struct, to: ffi::ptr, index: usize, value: B): none
//   {
//     :::ffistore[B](to, self.offsets()(index), self.sizes()(index), value)
//   }

//   alloc(self: struct): ffi::ptr
//   {
//     :::malloc(self.size)
//   }

//   free(self: struct, ptr: ffi::ptr): none
//   {
//     :::free(ptr)
//   }
// }
