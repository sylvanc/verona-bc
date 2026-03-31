use cb = ffi::callback[u64->u64];
use uv_buf_type = ffi::struct[(ffi::ptr, usize)];

main(): i32
{
  let h = (x: u64): u64 -> x + 10;
  let f = cb h;
  let buf = uv_buf_type.alloc;

  uv_buf_type.store[usize](buf, 1, 7);
  let size = uv_buf_type.load[usize](buf, 1);
  uv_buf_type.free(buf);
  if size != 7 { 1 } else { 0 }
}
