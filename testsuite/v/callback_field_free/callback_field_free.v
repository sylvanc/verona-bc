holder
{
  cb: ffi::callback;

  create(): holder
  {
    let cb = ffi::callback (): none -> {}
    new {cb}
  }

  final(self: holder): none
  {
    self.cb.free
  }
}

main(): i32
{
  0
}
