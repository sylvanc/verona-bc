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

main(): none
{
  let h = holder();
  h.reset;
}
