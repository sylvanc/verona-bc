holder
{
  _cb: ffi::callback[ffi::ptr->none];

  create(): holder
  {
    new
    {
      _cb = ffi::callback (p: ffi::ptr): none -> {}
    }
  }

  reset(self: holder): none
  {
    self._cb = ffi::callback (p: ffi::ptr): none -> {}
  }
}

main(): i32
{
  let h = holder();
  h.reset;
  0
}
