main(): i32
{
  let buf = later.alloc;
  later.store[usize](buf, 0, 7);
  let size = later.load[usize](buf, 0);
  later.free(buf);
  var result = 0;

  if size != 7
  {
    result = result + 1
  }

  result
}
