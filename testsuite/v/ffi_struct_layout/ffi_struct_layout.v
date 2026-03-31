main(): i32
{
  let pair = ffi::struct[(u8, i32, usize)]::create();
  let single = ffi::struct[u64]::create();
  let pair_ptr = pair.alloc;
  let single_ptr = single.alloc;
  let pair_offsets = pair.offsets;
  let single_offsets = single.offsets;
  var result = i32 0;

  pair.store[u8](pair_ptr, 0, u8 7);
  pair.store[i32](pair_ptr, 1, 5);
  pair.store[usize](pair_ptr, 2, 1234);
  single.store[u64](single_ptr, 0, u64 99);

  if pair.load[u8](pair_ptr, 0) != u8 7 { result = result + i32 1 }
  if pair.load[i32](pair_ptr, 1) != i32 5 { result = result + i32 2 }
  if pair.load[usize](pair_ptr, 2) != 1234 { result = result + i32 4 }
  if single.load[u64](single_ptr, 0) != u64 99 { result = result + i32 8 }

  if pair_offsets(0) != 0 { result = result + i32 16 }
  if pair_offsets(1) <= pair_offsets(0) { result = result + i32 32 }
  if pair_offsets(2) <= pair_offsets(1) { result = result + i32 64 }
  if pair.size <= pair_offsets(2) { result = result + i32 128 }
  if single_offsets(0) != 0 { result = result + i32 256 }
  if single.size == 0 { result = result + i32 512 }

  pair.free(pair_ptr);
  single.free(single_ptr);
  result
}
