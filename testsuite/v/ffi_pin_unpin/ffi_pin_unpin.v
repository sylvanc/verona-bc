box
{
  data: array[u8];

  create(data: array[u8]): box
  {
    new {data}
  }
}

main(): none
{
  let b = box(array[u8]::fill(3, u8 1));
  let c = cown none;

  ffi::pin(b);
  ffi::unpin(b);
  ffi::pin(b.data);
  ffi::unpin(b.data);
  ffi::pin(c);
  ffi::unpin(c);
}
