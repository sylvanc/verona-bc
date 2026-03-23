holder
{
  cb: ffi::callback[() -> none];

  create(): holder
  {
    let cb = ffi::callback (): none -> {}
    new {cb}
  }
}

main(): i32
{
  let h = holder();
  0
}
