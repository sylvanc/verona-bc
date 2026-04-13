box_state
{
  _cb: ffi::callback[ffi::ptr->none];

  create(): box_state
  {
    let self = new
    {
      _cb = ffi::callback (p: ffi::ptr): none -> {}
    }

    self._cb = ffi::callback (p: ffi::ptr): none -> {}
    self
  }
}

holder
{
  c: cown[box_state];

  create(s: box_state): holder
  {
    new {c = cown s}
  }
}

main(): none
{
  let s = box_state();
  let h = holder(s);
}
